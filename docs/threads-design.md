# Design: Minimal Threading Primitives

Status: **designed; Thread declarations landed in `aether.h`, implementation pending** (updated 2026-07-13; revised to confine platform ifdefs to the `os_` layer — see decision 7).
Prerequisites already landed: public `atomic_load_acq_u64` / `atomic_store_rel_u64` (ATOMICS section), the SPSC-correct `RingBuffer`, and the `AETHER_API` linkage modes (extern / static / dllexport / dllimport).

## Motivation

Target use case: a network thread receives user commands and hands them to a
long-running worker thread; the worker responds to commands and periodically
does tightly-timed work.

```
[network thread]                       [worker thread]
recv() -> parse command                loop:
  -> ring_buffer_write(cmd_ring, ..)     semaphore_wait_timeout(&wake, ..)
  -> semaphore_post(&wake, 1)            drain cmd_ring (non-blocking reads)
                                         if timed-work active:
ring_buffer_read(rsp_ring)                 high_res_timer_wait(&t)
  -> send() response                       do step; poll cmd_ring between steps
                                           ring_buffer_write(rsp_ring, result)
```

Key pattern: during the tightly-timed phase the worker *stops blocking on the
semaphore* and switches to its `HighResTimer` cadence, polling the command
ring with a non-blocking read once per cycle (one acquire load when empty —
costs the timing loop nothing). `thread_set_priority` matters here: the
timer's `spin_margin` only helps if the scheduler runs the thread when the
wait expires.

## Scope

Threads, one wakeup primitive (semaphore), a mutex. Wrappers over OS
primitives only — the same category as the existing `os_` platform layer,
except these are *public* because no higher-level aether type covers them.

**Non-goals** (application architecture, not library primitives): thread
pools, job systems, futures, channels, TLS, detach, once-init.

**Deferred until they have a caller:** `atomic_add_u64` (fetch-add),
`atomic_cas_u64`, a *public* `cpu_relax()` (the internal `os_cpu_relax()`
already exists in the platform layer; only the public wrapper is deferred),
cache-line padding / Lamport index caching on the ring.

## Decisions and rationale

1. **Entry signature `int (*thread_fn)(void*)` with a stack handshake, no
   allocation.** Win32 gives one context pointer and wants
   `unsigned __stdcall (*)(void*)`, so a thunk must smuggle `{fn, user}`
   through. Instead of a hidden `malloc` (freed by the thunk), the context
   lives on the creator's stack; the new thread copies it out and
   release-stores a `taken` flag; the creator spins (with `os_thread_yield`)
   until then. Cost: `thread_create` blocks until the new thread is first
   scheduled (normally microseconds, worst case a scheduler quantum). Fine
   for long-lived workers created at startup; swap to malloc if create-heavy
   usage ever appears.
   `int` return type: it is the portable intersection of the OS join
   mechanisms (Win32 exit code is a 32-bit `DWORD`; pthreads returns `void*`,
   which is wider) and matches C11 `thrd_start_t`. Results wider than 32 bits
   go through the `user` context, not the exit code.

2. **`_beginthreadex`, not `CreateThread`.** aether uses the CRT (stdio);
   `_beginthreadex` initializes and frees per-thread CRT state. Lives in
   `<process.h>` (add next to `<windows.h>` inside the
   `#if AETHER_OS_WINDOWS` platform block of the implementation section).

3. **Handles are `void*` for all three types, for now.** `HANDLE` (thread,
   semaphore) and `SRWLOCK` (mutex) are pointer-sized; `{0}` is invalid for
   Thread/Semaphore and a *valid unlocked mutex* for Mutex (SRWLOCK zero-init;
   no destructor needed). Known future cost: a POSIX port grows
   `Semaphore`/`Mutex` to inline `u64 storage[N]` (`sem_t` = 32 B,
   `pthread_mutex_t` = 40 B on glibc x64) — a source-level change in a
   header-only library, so not paid today. Pin the assumption with
   `AETHER_STATIC_ASSERT(sizeof(SRWLOCK) <= sizeof(void*), "SRWLOCK exceeds Mutex storage")`
   (the macro takes a message argument).
   *Open question (2026-07-13):* a `u64` handle was floated (opaque, holds
   integer `pthread_t` on glibc without pointer casts). Technically a wash vs
   `void*`; the deciding factor is consistency — every other aether OS handle
   (`os_file_open`, `os_create_timer`) is `void*`. If `u64` handles are ever
   adopted, adopt them library-wide (os layer included), not for `Thread`
   alone. Note `Mutex` is different regardless: `SRWLOCK` is *inline storage*
   zero-initialized in place, not a handle returned by the OS.

4. **Semaphore over condition variable** as the single wakeup primitive
   (decided during the SPSC design discussion): a condvar needs a mutex, a
   predicate, and spurious-wakeup handling; a semaphore is one counter.
   Paired with the SPSC ring it is the classic message queue. Given up:
   broadcast wake — not needed for a two-thread design.

5. **Three priority levels only** (`Normal`, `High`, `TimeCritical`); a full
   ladder invites unjustifiable tuning. Win32 mapping: `NORMAL` / `HIGHEST` /
   `TIME_CRITICAL`.

6. **Naming hazard, decided consciously:** aether convention is `*_release` =
   destructor, but Win32/classic semaphore vocabulary uses "release" for the
   post/V operation (`ReleaseSemaphore`). Keep the aether convention —
   `semaphore_post` signals, `semaphore_release` destroys — with a warning
   comment on the declaration.

7. **Platform ifdefs live in the `os_` layer only** (revised; the first
   draft put them in the public function bodies, which broke the pattern the
   rest of aether follows). Public THREADS functions are platform-free thin
   wrappers over `internal os_thread_*` / `os_semaphore_*` / `os_mutex_*`
   helpers — exactly like memory, files, and timers. Inside each helper the
   house style holds: one `#if AETHER_OS_WINDOWS` branch (not
   `#ifdef _WIN32`) with a per-function `#error` on unsupported platforms, so
   ports get an enumerated to-do list at compile time. POSIX branches are
   `#error` until a real port.
   Cost accepted: one-liners (`thread_yield`, the mutex ops) become a wrapper
   around a wrapper. Uniformity of the rule — *public code never sees
   `windows.h` types or platform ifdefs* — is worth more than the saved
   lines; same trade already made for `os_cpu_relax`.
   **The seam inside thread creation:** the handshake (`ThreadStart_` + the
   spin on `taken`) is platform-neutral and written **once**, inside
   `os_thread_create`; only the thunk and the create call sit under the
   ifdef, because the thunk's calling convention belongs to the platform
   (`unsigned __stdcall (*)(void*)` on Win32, `void* (*)(void*)` on
   pthreads). Each platform branch owns its own thunk adapting to the shared
   `ThreadStart_`.
   Naming: noun-verb (`os_thread_create`, `os_semaphore_post`) — the layer
   currently has both orderings (`os_create_timer`, `os_file_open`); noun-verb
   groups these multi-function families.

8. **Linkage and internal semantics** (post `AETHER_API` linkage modes):
   public functions carry `AETHER_API` on both the declaration *and* the
   definition — required because `AETHER_STATIC` expands it to `static` and
   `AETHER_BUILD_DLL` to `dllexport`, which must appear on the definition.
   The `os_` helpers and the thunk use the `internal` macro, not bare
   `static`, and keep the trailing-underscore convention for file-local
   types/functions that are not part of the os_ surface (`ThreadStart_`,
   `os_thread_thunk_`). Prototypes use `(void)`, not empty parens.

## Public API (header, THREADS section after TIMERS)

The Thread half below is already declared in `aether.h`; Semaphore and Mutex
are not yet declared.

```c
/* ------- T H R E A D S ----------------------------------------------------- */

typedef int (*thread_fn)(void* user);

typedef struct Thread { void* handle; } Thread;   /* {0} = invalid */

typedef u8 ThreadPriority;

enum ThreadPriority_
{
    ThreadPriority_Normal = 0,
    ThreadPriority_High,          /* above normal; pre-empts normal threads */
    ThreadPriority_TimeCritical,  /* for tightly-timed loops; use sparingly */
};

/* thread_create blocks until the new thread has started and copied fn/user
   (no allocation). Returns {0} on failure. */
AETHER_API Thread thread_create(thread_fn fn, void* user);
AETHER_API int    thread_join(Thread* t);    /* waits, closes the handle, returns fn's result */
AETHER_API b8     thread_set_priority(Thread* t, ThreadPriority p);
AETHER_API void   thread_yield(void);
AETHER_API void   thread_sleep_ms(u32 ms);   /* coarse (~15 ms granularity); use HighResTimer for pacing */

/* Counting semaphore. NOTE: post() is the classic "release"/V operation;
   semaphore_release() DESTROYS the semaphore (aether convention). */
typedef struct Semaphore { void* handle; } Semaphore;   /* {0} = invalid */

AETHER_API Semaphore semaphore_create(u32 initial);
AETHER_API void      semaphore_release(Semaphore* s);
AETHER_API void      semaphore_post(Semaphore* s, u32 count);
AETHER_API void      semaphore_wait(Semaphore* s);
AETHER_API b8        semaphore_wait_timeout(Semaphore* s, u32 ms);  /* false on timeout */

/* Mutex. Zero-init is a valid unlocked mutex; no destructor needed. */
typedef struct Mutex { void* srw; } Mutex;

AETHER_API void mutex_lock(Mutex* m);
AETHER_API b8   mutex_try_lock(Mutex* m);
AETHER_API void mutex_unlock(Mutex* m);
```

## Internal `os_` surface (platform layer, after `os_cpu_relax`)

All platform ifdefs for THREADS live behind these. `ThreadPriority` is a
public type declared above the implementation section, so the os layer may
take it directly. Atomics are declared in the header, so `os_thread_create`
may call them regardless of definition order.

```c
internal void* os_thread_create(thread_fn fn, void* user); /* NULL on failure; blocks for handshake */
internal int   os_thread_join(void* h);                    /* waits, closes handle, returns exit code */
internal b8    os_thread_set_priority(void* h, ThreadPriority p);
internal void  os_thread_yield(void);
internal void  os_thread_sleep_ms(u32 ms);

internal void* os_semaphore_create(u32 initial);           /* NULL on failure */
internal void  os_semaphore_release(void* h);              /* destroys (closes handle) */
internal void  os_semaphore_post(void* h, u32 count);
internal void  os_semaphore_wait(void* h);
internal b8    os_semaphore_wait_timeout(void* h, u32 ms); /* false on timeout */

internal void  os_mutex_lock(void** storage);              /* storage = &Mutex.srw (inline SRWLOCK) */
internal b8    os_mutex_try_lock(void** storage);
internal void  os_mutex_unlock(void** storage);
```

## Reference implementation sketch

Two blocks: the `os_` helpers go in the platform section after
`os_cpu_relax`; the public wrappers go in a THREADS block after TIMING,
mirroring declaration order. Only the os_ helpers contain
`#if AETHER_OS_WINDOWS`; public definitions carry `AETHER_API` (decision 8)
and are platform-free.

### os_ layer (platform section)

```c
typedef struct ThreadStart_
{
    thread_fn fn;
    void*     user;
    u64       taken;   /* handshake: set by the new thread once fn/user are copied */
} ThreadStart_;

#if AETHER_OS_WINDOWS
internal unsigned __stdcall os_thread_thunk_(void* arg)
{
    ThreadStart_* start = (ThreadStart_*)arg;
    thread_fn fn   = start->fn;
    void*     user = start->user;
    atomic_store_rel_u64(&start->taken, 1);  /* creator's stack frame dies after this */
    return (unsigned)fn(user);
}
#endif // AETHER_OS_WINDOWS

internal void os_thread_yield(void)
{
#if AETHER_OS_WINDOWS
    SwitchToThread();
#else
    #error "AETHER: OS thread yield not implemented for this platform"
#endif
}

internal void* os_thread_create(thread_fn fn, void* user)
{
    /* handshake is platform-neutral, written once; only the thunk and the
       create call are per-platform */
    ThreadStart_ start = {0};
    start.fn   = fn;
    start.user = user;

    void* h = NULL;
#if AETHER_OS_WINDOWS
    h = (void*)_beginthreadex(NULL, 0, os_thread_thunk_, &start, 0, NULL);
#else
    #error "AETHER: OS thread create not implemented for this platform"
#endif
    if (!h) return NULL;

    while (!atomic_load_acq_u64(&start.taken))
        os_thread_yield();

    return h;
}

internal int os_thread_join(void* h)
{
#if AETHER_OS_WINDOWS
    WaitForSingleObject((HANDLE)h, INFINITE);
    DWORD code = 0;
    GetExitCodeThread((HANDLE)h, &code);   /* race-free: the wait guarantees termination */
    CloseHandle((HANDLE)h);
    return (int)code;
#else
    #error "AETHER: OS thread join not implemented for this platform"
#endif
}

internal b8 os_thread_set_priority(void* h, ThreadPriority p)
{
#if AETHER_OS_WINDOWS
    int win = THREAD_PRIORITY_NORMAL;
    if (p == ThreadPriority_High)         win = THREAD_PRIORITY_HIGHEST;
    if (p == ThreadPriority_TimeCritical) win = THREAD_PRIORITY_TIME_CRITICAL;
    return SetThreadPriority((HANDLE)h, win) != 0;
#else
    #error "AETHER: OS thread priority not implemented for this platform"
#endif
}

internal void os_thread_sleep_ms(u32 ms)
{
#if AETHER_OS_WINDOWS
    Sleep(ms);
#else
    #error "AETHER: OS thread sleep not implemented for this platform"
#endif
}

internal void* os_semaphore_create(u32 initial)
{
#if AETHER_OS_WINDOWS
    return (void*)CreateSemaphoreW(NULL, (LONG)initial, 0x7FFFFFFF, NULL);
#else
    #error "AETHER: OS semaphore create not implemented for this platform"
#endif
}

internal void os_semaphore_release(void* h)
{
#if AETHER_OS_WINDOWS
    CloseHandle((HANDLE)h);
#else
    #error "AETHER: OS semaphore release not implemented for this platform"
#endif
}

internal void os_semaphore_post(void* h, u32 count)
{
#if AETHER_OS_WINDOWS
    ReleaseSemaphore((HANDLE)h, (LONG)count, NULL);
#else
    #error "AETHER: OS semaphore post not implemented for this platform"
#endif
}

internal void os_semaphore_wait(void* h)
{
#if AETHER_OS_WINDOWS
    WaitForSingleObject((HANDLE)h, INFINITE);
#else
    #error "AETHER: OS semaphore wait not implemented for this platform"
#endif
}

internal b8 os_semaphore_wait_timeout(void* h, u32 ms)
{
#if AETHER_OS_WINDOWS
    return WaitForSingleObject((HANDLE)h, ms) == WAIT_OBJECT_0;
#else
    #error "AETHER: OS semaphore wait not implemented for this platform"
#endif
}

internal void os_mutex_lock(void** storage)
{
#if AETHER_OS_WINDOWS
    AcquireSRWLockExclusive((SRWLOCK*)storage);
#else
    #error "AETHER: OS mutex lock not implemented for this platform"
#endif
}

internal b8 os_mutex_try_lock(void** storage)
{
#if AETHER_OS_WINDOWS
    return TryAcquireSRWLockExclusive((SRWLOCK*)storage) != 0;
#else
    #error "AETHER: OS mutex try-lock not implemented for this platform"
#endif
}

internal void os_mutex_unlock(void** storage)
{
#if AETHER_OS_WINDOWS
    ReleaseSRWLockExclusive((SRWLOCK*)storage);
#else
    #error "AETHER: OS mutex unlock not implemented for this platform"
#endif
}
```

### Public wrappers (THREADS block after TIMING) — no platform ifdefs

```c
/* ------------------------------------------------------------------------- */
/* --- T H R E A D S ------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

AETHER_API Thread thread_create(thread_fn fn, void* user)
{
    Thread t = {0};
    if (!fn) return t;
    t.handle = os_thread_create(fn, user);
    return t;
}

AETHER_API int thread_join(Thread* t)
{
    if (!t || !t->handle) return -1;
    int code = os_thread_join(t->handle);
    t->handle = NULL;
    return code;
}

AETHER_API b8 thread_set_priority(Thread* t, ThreadPriority p)
{
    if (!t || !t->handle) return false;
    return os_thread_set_priority(t->handle, p);
}

AETHER_API void thread_yield(void)
{
    os_thread_yield();
}

AETHER_API void thread_sleep_ms(u32 ms)
{
    os_thread_sleep_ms(ms);
}

AETHER_API Semaphore semaphore_create(u32 initial)
{
    Semaphore s = {0};
    s.handle = os_semaphore_create(initial);
    return s;
}

AETHER_API void semaphore_release(Semaphore* s)
{
    if (!s || !s->handle) return;
    os_semaphore_release(s->handle);
    s->handle = NULL;
}

AETHER_API void semaphore_post(Semaphore* s, u32 count)
{
    if (!s || !s->handle || !count) return;
    os_semaphore_post(s->handle, count);
}

AETHER_API void semaphore_wait(Semaphore* s)
{
    if (!s || !s->handle) return;
    os_semaphore_wait(s->handle);
}

AETHER_API b8 semaphore_wait_timeout(Semaphore* s, u32 ms)
{
    if (!s || !s->handle) return false;
    return os_semaphore_wait_timeout(s->handle, ms);
}

AETHER_API void mutex_lock(Mutex* m)
{
    if (!m) return;
    os_mutex_lock(&m->srw);
}

AETHER_API b8 mutex_try_lock(Mutex* m)
{
    if (!m) return false;
    return os_mutex_try_lock(&m->srw);
}

AETHER_API void mutex_unlock(Mutex* m)
{
    if (!m) return;
    os_mutex_unlock(&m->srw);
}
```

## Pitfalls to preserve when implementing

- **Thunk order is load-bearing:** copy `fn`/`user` to locals *before* the
  release store of `taken`. After `taken = 1` the creator may return and
  `start` (on `os_thread_create`'s stack) dangles. Getting this wrong is a
  use-after-free that passes tests (the creator usually loses the race
  slowly) and fails in production.
- **Spin with `os_thread_yield` (`SwitchToThread`), not `Sleep(0)`:**
  `Sleep(0)` only yields to equal-or-higher-priority threads and would starve
  a lower-priority new thread; `SwitchToThread` yields to any ready thread.
- **`(unsigned)fn(user)`:** Win32 exit codes are unsigned 32-bit. Negative
  `int` returns round-trip correctly through `GetExitCodeThread` + cast back;
  values >= 2^31 don't exist in the `int` API. Also: never return 259
  (`STILL_ACTIVE`) as a real status.
- **`GetExitCodeThread` after `WaitForSingleObject` is race-free** — the wait
  guarantees termination, so `STILL_ACTIVE` cannot be observed.
- **SRWLOCK is non-recursive:** relocking from the same thread deadlocks.
  Right default (recursion hides broken lock discipline) — document in README.
- The mutex is almost optional for the target architecture (all cross-thread
  data flows through SPSC rings), but it is ~15 lines and the first shared
  table will want it.

## Validation plan

- `thread_create`/`join` round trip: N threads returning distinct codes, all
  joined, codes verified.
- Mutex: 4 threads x 100k increments of a shared counter under lock == 400k.
  (Do not add the racy no-lock control as a suite case — flaky by design.)
- Semaphore: worker blocks on `wait`, main posts K times, worker consumes
  exactly K; `wait_timeout(50)` on an empty semaphore returns `false` with
  `time_mark`/`time_elapsed_sec` confirming >= 50 ms elapsed.
- Threads-record-results-into-structs, main-thread-asserts-after-join (the
  harness `ASSERT` counters are not thread-safe — same pattern as
  `spsc_stress` in `tests/test_ring.c`).
- **Do not convert `spsc_stress`'s ad-hoc thread wrappers to this API yet**:
  its pthread branch exists so the ring test compiles under
  `clang -fsanitize=thread` on Linux/WSL, and this API is `#error` on POSIX
  until ported. Converting would silently kill the TSan path (which is still
  an outstanding validation step for the ring itself).

## Follow-ups after landing

- README: new Threads section (mirror the Atomics/Ring pattern), including
  the SRWLOCK non-recursive note and the semaphore naming caveat.
- CHANGELOG: under Added.
- Later, with callers: `atomic_add_u64` / `atomic_cas_u64`, `cpu_relax()`;
  POSIX port (grows `Semaphore`/`Mutex` to inline storage; enables running
  the whole thread test suite under TSan).
