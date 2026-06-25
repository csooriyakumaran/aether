# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.0.7] - 2026-06-25

### Added
- `RingBuffer` with `ring_buffer_alloc` / `ring_buffer_write` / `ring_buffer_peek` / `ring_buffer_read` / `ring_buffer_release`: fixed-capacity single-producer/single-consumer byte ring buffer using a virtually-mirrored ("magic") mapping — the same physical pages are mapped twice back-to-back, so any span up to capacity wraps as a single `memcpy` and `ring_buffer_peek` returns a contiguous view even when the data straddles the seam. Capacity rounds up to a power of two ≥ the OS allocation granularity (64 KiB on Windows); `read`/`write` are monotonic counters masked on access, which keeps full/empty unambiguous and stays correct across 64-bit overflow. Covered in `tests/test_ring.c`.
- Windows 10 1803+ only: the backing `VirtualAlloc2` / `MapViewOfFile3` / `UnmapViewOfFile2` are resolved at runtime via `GetProcAddress` from `kernelbase.dll`, so there is no extra link dependency and the binary still loads on older Windows — `ring_buffer_alloc` simply returns `false`. The `MEM_*_PLACEHOLDER` constants are `#ifndef`-supplied so the header compiles against older SDKs.

## [0.0.6] - 2026-06-24

### Added
- `str8_eq` / `str8_cmp` / `str8_slice` / `str8_trim`: string-view operations — equality, three-way ordering, half-open `[start, end)` slicing, and leading/trailing whitespace trim. All operate on `str8_view` and allocate nothing.
- `ArenaTemp` with `arena_begin_temp` / `arena_end_temp`: scoped checkpoint/restore over an existing arena, nests LIFO. Names the manual `mark = arena.pos; ... arena_pop_to(...)` pattern; ending out of order clamps safely rather than corrupting `pos`.
- `AETHER_NO_ASSERT` / `AETHER_NO_MINMAX` / `AETHER_NO_ARRAY_COUNT`: opt-out defines so a consumer can suppress aether's `ASSERT`/`MIN`/`MAX`/`ARRAY_COUNT` and supply their own. The public macros also now defer to any pre-existing definition via `#ifndef` guards.

### Changed
- `FATAL` no longer calls `abort()` — it relies on `DEBUG_BREAK()` alone, dropping the `<stdlib.h>` dependency. (`__builtin_trap()` already never returns on GCC/Clang; `__debugbreak()` + the default unhandled-exception path terminates on MSVC.)
- Internal macros now resolve through private `AETHER_*_` names (`AETHER_ASSERT_`, `AETHER_MIN_`, `AETHER_MAX_`, `AETHER_ARRAY_COUNT_`) so the library's own code never depends on a consumer-overridable public name.

## [0.0.5] - 2026-06-23

### Added
- `bytes` / `bytes_view`: new base byte-span types (mutable / read-only) underlying `str8`/`str8_view`, with `view_from_bytes`/`view_from_str8` helpers to convert a mutable span to its read-only view.
- `c_str`: convert a `str8_view` to a null-terminated, arena-allocated C string.

### Changed
- **Breaking:** `cstr8` is removed; `str8` and the new `str8_view` are now aliases for `bytes`/`bytes_view`. `STR(...)` now produces a `str8_view` instead of a `cstr8`.
- **Breaking:** `arena_read_file` now returns `bytes` instead of `str8`.
- **Breaking:** `map_file` now returns `bytes_view` instead of `str8`, and `unmap_file` takes a `bytes_view` — reflecting that a memory-mapped file is a read-only, non-owning view.
- `os_file_unmap` takes `const void*` instead of `void*`.
- `aether/aether-version.h` is no longer included automatically by `aether.h`; consumers that need version macros must include the generated header themselves.

## [0.0.4] - 2026-06-22

### Added
- `map_file` / `unmap_file`: read-only memory-mapped file access (`os_file_map`/`os_file_unmap` on Windows, via `CreateFileMapping`/`MapViewOfFile`).
- `time_mark` / `time_elapsed_sec`: minimal high-resolution timing primitives, backed by `QueryPerformanceCounter`/`QueryPerformanceFrequency` on Windows.
- `FILE_FLAG_SEQUENTIAL_SCAN` hint on read-only file opens, matching aether's whole-file sequential read/parse access pattern.
- Split `os_file_open` into `os_file_open_for_read` and `os_file_open_for_write` (write path not yet implemented).

### Fixed
- **Critical:** removed a stray `#define AETHER_IMPLEMENTATION` that caused the implementation to be compiled into every translation unit including the header, breaking the single-header contract for any consumer with more than one `.c`/`.cpp` file including it. This defect is present in the published `v0.0.3` tag.
- `<stdio.h>` was only conditionally included under `AETHER_ENABLE_ASSERTS`, while the always-on `FATAL` macro depends on it unconditionally. Now included unconditionally.
- `os_file_read` capped each `ReadFile` call at `MAXWORD` (64 KB − 1) instead of `MAXDWORD` (~4 GB), turning large sequential reads into thousands of small syscalls instead of one.
- `os_file_size` leaked its file handle if `GetFileSizeEx` failed.
- File mapping now handles zero-byte files explicitly; `CreateFileMapping` fails on empty files, which was previously indistinguishable from a real error.
- File primitives now call `CreateFileA` explicitly instead of the `CreateFile` macro, which resolves to `CreateFileW` under consumer builds that define `UNICODE`.

## [0.0.3] - 2026-06-20

### Added
- `arena_read_file`: read an entire file into an arena-backed buffer in a single allocation and read.
- Internal Windows file primitives backing it: `os_file_open`, `os_file_close`, `os_file_size`, `os_file_read`.
- Internal Windows timing primitives: `os_time_now`, `os_time_frequency`.
- Scaffolding for a memory-mapped file API (declared, not yet implemented — completed in the next release).

## [0.0.2] - 2026-06-20

### Fixed
- String helpers (`arena_push_cstring`, `arena_push_cstring_fmtv`, `arena_push_str8_copy`, `arena_push_str8_from_cstring`, `arena_push_str8_fmtv`) passed raw integer literals where an `ArenaZero` enum was expected. Legal in C, a hard compile error in C++.

## [0.0.1] - 2026-06-20

### Added
- Initial release: fixed-width type aliases, assertion/utility macros (`ASSERT`, `FATAL`, `BIT`, `KB`/`MB`/`GB`/`TB`, etc.), `str8`/`cstr8` string views, and a virtual-memory-backed linear arena allocator.
- Arena API: `arena_alloc`/`arena_alloc_ex`, `arena_push` and typed/array push macros, `arena_pop`/`arena_pop_to`/`arena_clear`/`arena_release`.
- `ArenaFlags` (decommit, chunked commit, always-zero, debug-fill-on-clear) and tri-state `ArenaZero` push policy (`FollowPolicy`/`Force`/`Never`).
- CMake install rules, test suite, and example.
