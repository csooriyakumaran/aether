# POSIX Port: Itemized Work List

Status: **inventory only, no port work started** (created 2026-07-13).
Every platform-dependent site in `aether.h` follows the house pattern — a
`#if AETHER_OS_WINDOWS` branch with an `#error` fallback — so a POSIX build
enumerates its own to-do list at compile time. This doc is that list written
down, with the intended POSIX primitive per item and the gotchas that are
*not* mechanical, so the port can be planned without a Linux compile handy.

First target: **Linux (WSL, glibc, clang)** — this unblocks the outstanding
TSan validation of the ring buffer and (once ported) the thread test suite.
macOS/BSD later; items that differ on those are flagged.

Items are referenced by function name, not line number — the file is in flux.

## 0. Global gates (must be lifted first)

| Site | Today | POSIX fix |
| --- | --- | --- |
| `#error "AETHER: Currently only supports Windows"` (top of header, after the `AETHER_API` block) | Hard stop on any non-Windows build | Delete or extend to `#if !(AETHER_OS_WINDOWS \|\| AETHER_OS_LINUX)` while the port is Linux-only |
| Platform includes block (implementation section, `#if AETHER_OS_WINDOWS` wrapping `<windows.h>`, `<process.h>`, the `VirtualAlloc2`/`MapViewOfFile3` typedefs and `MEM_*_PLACEHOLDER` defines) | Windows only | Add a sibling `#elif AETHER_OS_LINUX` block: `<pthread.h> <semaphore.h> <unistd.h> <fcntl.h> <sys/mman.h> <sys/stat.h> <time.h> <sched.h> <errno.h> <string.h>`; Linux ring buffer additionally wants `<sys/syscall.h>` or `memfd_create` via `<sys/mman.h>` (glibc >= 2.27, needs `_GNU_SOURCE`) |
| `_GNU_SOURCE` | n/a | Decide once: `memfd_create`, `pthread_setname_np` (future) and friends need it defined before any include. Either require the user to define it, or define it inside the implementation guard before the includes with a comment. |

**Already POSIX-ready, no work:** `AETHER_DLL_EXPORT/IMPORT`
(`__attribute__((visibility("default")))` branch exists); the ATOMICS
dispatch (`AETHER_ATOMICS_GNU` branch via `__atomic_*` builtins covers
GCC/Clang on any arch — needs TSan *validation*, not porting); OS/arch/
compiler detection macros (`AETHER_OS_LINUX` etc. already defined).

## 1. Error helpers (currently commented out)

| Function | Win32 today | POSIX fix |
| --- | --- | --- |
| `os_get_last_error` | `GetLastError()` | `errno` (return `(u32)errno`) |
| `os_error_string` | `FormatMessageA` | `strerror_r` — note the XSI vs GNU signature clash; with `_GNU_SOURCE` on glibc you get the GNU variant returning `char*`. Pin one with a cast-free wrapper. |

## 2. Memory (`os_mem_*`)

| Function | Win32 today | POSIX fix |
| --- | --- | --- |
| `os_mem_pagesize` | `GetSystemInfo` `.dwPageSize` | `sysconf(_SC_PAGESIZE)` |
| `os_mem_pagegranularity` | `.dwAllocationGranularity` (64 KiB) | Same as page size on POSIX — return `sysconf(_SC_PAGESIZE)`. Check no caller assumes granularity > pagesize. |
| `os_mem_reserve` | `VirtualAlloc(MEM_RESERVE, PAGE_READWRITE)` | `mmap(NULL, size, PROT_NONE, MAP_PRIVATE\|MAP_ANONYMOUS, -1, 0)` — `PROT_NONE` is the honest "reserved, not committed" analogue. Failure is `MAP_FAILED` (== `(void*)-1`), **not NULL** — normalize to NULL at this boundary. |
| `os_mem_commit` | `VirtualAlloc(MEM_COMMIT)` | `mprotect(ptr, size, PROT_READ\|PROT_WRITE)` — pages materialize on first touch (overcommit). Same contract as Windows for the arena's purposes. |
| `os_mem_decommit` | `VirtualFree(MEM_DECOMMIT)` | `madvise(ptr, size, MADV_DONTNEED)` then `mprotect(PROT_NONE)` — both, to release the pages *and* restore the fault-on-touch guarantee. |
| `os_mem_release` | `VirtualFree(0, MEM_RELEASE)` | `munmap(ptr, size)` — POSIX **needs the size**; the signature already carries it (Windows branch `(void)size`s it), so no API change. |

## 3. Files (`os_file_*`)

| Function | Win32 today | POSIX fix |
| --- | --- | --- |
| `os_file_open_for_read` | `CreateFileA(GENERIC_READ, ...)` | `open(path, O_RDONLY)` |
| `os_file_open_for_write` | `CreateFileA(GENERIC_WRITE, CREATE_ALWAYS, ...)` | `open(path, O_WRONLY\|O_CREAT\|O_TRUNC, 0644)` |
| `os_file_close` | `CloseHandle` | `close` |
| `os_file_size` | `GetFileSizeEx` | `fstat` → `st_size` |
| `os_file_read` | `ReadFile` loop | `read` loop — must handle partial reads **and `EINTR`** (retry); Windows didn't need the EINTR case |
| `os_file_write` | `WriteFile` loop | `write` loop — same partial/`EINTR` handling |
| `os_file_map` | `CreateFileMappingA` + `MapViewOfFile` | `mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)` — one call, no separate mapping object to close |
| `os_file_unmap` | `UnmapViewOfFile` (`(void)size`) | `munmap((void*)ptr, size)` — size already in the signature |

**Cross-cutting gotcha — fd in a `void*` handle:** POSIX file handles are
`int` fds and **fd 0 is valid**, but the `os_file_*` contract uses
`NULL`/`(void*)` as the failure sentinel. Casting the fd straight into the
pointer makes fd 0 indistinguishable from failure. Fix at the os_ boundary,
invisibly to callers: store `(void*)(intptr_t)(fd + 1)` and decode with
`(int)(intptr_t)h - 1`. Do **not** change the public sentinel convention for
this.

## 4. Ring-buffer memory (`os_*_ring`)

The Windows implementation uses the placeholder dance
(`VirtualAlloc2(MEM_RESERVE_PLACEHOLDER)` → split → `MapViewOfFile3` twice).
POSIX gets the same mirrored mapping with a different, simpler shape, which
means the *call sequence* doesn't map 1:1 — expect to restructure inside the
os_ functions, not just translate line by line:

| Function | Win32 today | POSIX fix (Linux) |
| --- | --- | --- |
| `os_mem_reserve_ring` | `VirtualAlloc2` placeholder, 2×size | `mmap(NULL, 2*size, PROT_NONE, MAP_PRIVATE\|MAP_ANONYMOUS, -1, 0)` — just claims the address range |
| `os_mem_split_ring` | `VirtualFree(MEM_PRESERVE_PLACEHOLDER)` split | **No-op on POSIX** (`MAP_FIXED` below overwrites in place; return true) |
| `os_mem_map_ring` | `CreateFileMappingA` + `MapViewOfFile3` ×2 | `memfd_create("aether_ring", 0)` + `ftruncate(fd, size)` + two `mmap(base / base+size, size, PROT_READ\|PROT_WRITE, MAP_SHARED\|MAP_FIXED, fd, 0)`; `close(fd)` immediately after — mappings keep the memory alive, so no fd needs storing |
| `os_mem_release_ring` | `UnmapViewOfFile2` / `VirtualFree` | single `munmap(ptr, 2*size)` covers both halves |

macOS/BSD note: no `memfd_create`. Portable fallback is
`shm_open` + `shm_unlink` immediately after (same anonymous-fd effect), or
`mkstemp` on a tmpfs path as a last resort. Defer until a mac port is real.

Validation: the existing ring tests (`tests/test_ring.c`) exercise the
mirror property; run them under `clang -fsanitize=thread` on WSL — this is
the outstanding TSan item and half the motivation for the port.

## 5. Time and timers (`os_time_*`, `os_*_timer`)

| Function | Win32 today | POSIX fix |
| --- | --- | --- |
| `os_time_now` | `QueryPerformanceCounter` | `clock_gettime(CLOCK_MONOTONIC)` → `ts.tv_sec * 1000000000ull + ts.tv_nsec` |
| `os_time_frequency` | `QueryPerformanceFrequency` | return `1000000000ull` (ticks are nanoseconds) |
| `os_create_timer` | `CreateWaitableTimerExW(HIGH_RESOLUTION)` | See decision below |
| `os_timer_sleep` | `SetWaitableTimer` + `WaitForSingleObject` | `clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &rem)` — loop on `EINTR` with the remainder |
| `os_timer_release` | `CloseHandle` | Depends on the decision below (no-op or `close`) |

**Decision needed — timer handle on POSIX:** `clock_nanosleep` needs no
kernel object at all, so the simplest port makes `os_create_timer` return a
non-NULL dummy (e.g. `(void*)1`) and `os_timer_release` a no-op. The
alternative, `timerfd_create`, buys a real fd (pollable, could later join an
epoll set) but is **Linux-only** and adds nothing for the current
`HighResTimer` use (blocking wait + spin margin). Recommend: dummy handle +
`clock_nanosleep`; revisit if a poll-based event loop ever appears.
Sleep resolution note: Linux `clock_nanosleep` typically wakes within tens of
microseconds (no 15 ms Windows quantum), so `HighResTimer.spin_margin`
tuning may want a smaller platform default — measure before changing.

## 6. CPU / scheduling

| Function | Win32 today | POSIX fix |
| --- | --- | --- |
| `os_cpu_relax` | `YieldProcessor()` | Not an OS call — compiler/arch: `__builtin_ia32_pause()` (x64) / `__asm__ volatile("yield")` (arm64) under the GCC/Clang branch. Consider keying this one on compiler+arch rather than OS. |
| `os_thread_yield` | `SwitchToThread()` | `sched_yield()` |

## 7. Threads / semaphore / mutex (Windows impl in progress — see `threads-design.md`)

| Function | Win32 (per design doc) | POSIX fix |
| --- | --- | --- |
| `os_thread_thunk_` | `unsigned __stdcall (*)(void*)` | Sibling thunk `void* (*)(void*)` under `#elif`; same `ThreadStart_` handshake (platform-neutral, already shared); return `(void*)(intptr_t)fn(user)` |
| `os_thread_create` | `_beginthreadex` | `pthread_create` — `pthread_t` on glibc is `unsigned long`, not a pointer; round-trip through the handle via `(void*)`/`uintptr_t` casts (or see the `u64`-handle open question in `threads-design.md` decision 3). `pthread_t` 0 is not a guaranteed-invalid value, but with the NULL-handle convention a real thread whose `pthread_t` happens to be 0 would look invalid — same class of problem as fd 0; same `+1` encoding trick applies if it ever bites. |
| `os_thread_join` | `WaitForSingleObject` + `GetExitCodeThread` + `CloseHandle` | `pthread_join(t, &ret)` → `(int)(intptr_t)ret` — one call does wait + reap; no separate close |
| `os_thread_set_priority` | `SetThreadPriority` | **The hard one.** Raising priority needs a realtime policy: `pthread_setschedparam(t, SCHED_FIFO, ...)` — which fails with `EPERM` for unprivileged processes unless `RLIMIT_RTPRIO`/`CAP_SYS_NICE` is granted. `nice` values don't apply per-thread portably. Port as: map High/TimeCritical to `SCHED_FIFO` prio 1/99-ish, return `false` on `EPERM`, and document that callers must tolerate `false` (the `b8` return already allows this — keep it honest, don't fake success). |
| `os_thread_sleep_ms` | `Sleep(ms)` | `nanosleep` loop on `EINTR` (or reuse `os_timer_sleep`'s clock_nanosleep path) |
| `os_semaphore_create/release/post/wait/wait_timeout` | `CreateSemaphoreW` / `CloseHandle` / `ReleaseSemaphore` / `WaitForSingleObject(ms)` | Unnamed `sem_init` / `sem_destroy` / `sem_post` ×count / `sem_wait` (retry `EINTR`) / `sem_timedwait` — **but `sem_timedwait` takes an absolute `CLOCK_REALTIME` deadline** (wall-clock jumps affect it; `sem_clockwait` with `CLOCK_MONOTONIC` is glibc 2.30+). Also `sem_t` is 32 bytes: **does not fit the `void* handle`** — this is the known struct-growth cost from `threads-design.md` decision 3 (`u64 storage[4]` inline, `{0}` no longer guaranteed-invalid → semaphore gains an explicit validity convention or an `initialized` flag). macOS: unnamed POSIX semaphores are **not supported** (`sem_init` returns `ENOSYS`) — needs GCD `dispatch_semaphore` or named `sem_open`; defer with the mac port. |
| `os_mutex_lock/try_lock/unlock` | `AcquireSRWLockExclusive` etc. on inline `void*` storage | `pthread_mutex_t` is 40 bytes on glibc x64 — same struct growth (`u64 storage[5]`). Zero-init: `PTHREAD_MUTEX_INITIALIZER` is all-zero on glibc **in practice but not by contract**; either accept the glibc assumption with a static assert + comment, or add `mutex_init`. This threatens the "zero-init is a valid unlocked mutex, no destructor" property from the design doc — resolve consciously, in the doc, before porting. |

## 8. Outside `aether.h`

- `tests/test_ring.c` `spsc_stress`: already has a hand-rolled pthread
  branch (exists precisely so TSan can run before this port). After the
  thread API is ported and TSan-clean, convert it to `thread_create`/`join`
  and delete the ad-hoc wrappers.
- Build: no build system changes expected (header-only); test compile lines
  on Linux need `-lpthread` only for older glibc (< 2.34 folds it into libc).

## Suggested order of attack

1. Lift the global gate; add the Linux includes block.
2. Memory + files + time (mechanical, unblocks everything else; arena and
   str8 tests should pass at this point).
3. Ring memory (`memfd_create` path) → run ring tests under TSan on WSL
   (closes the outstanding ring validation item).
4. Threads/semaphore/mutex — after resolving the two flagged design points
   (Semaphore/Mutex inline storage growth; zero-init mutex contract).
5. Timer path + `os_cpu_relax` arch branches.
6. Convert `spsc_stress` to the public thread API; full suite under TSan.
