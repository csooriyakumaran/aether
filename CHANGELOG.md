# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- String operations completing the `str8` API, grouped by allocation behavior:
  - queries (no allocation): `str8_eq_nocase` (ASCII case-insensitive equality), `str8_has_suffix`, `str8_find` / `str8_find_last` / `str8_find_char` (`str8_find_last` reports the last *non-overlapping* occurrence; an empty needle yields `s.size`).
  - views (no allocation): `str8_skip` / `str8_drop` (view without the first/last `n` bytes), `str8_trim_left` / `str8_trim_right`.
  - lists: `Str8List` / `Str8Node` / `Str8Array` with `str8_split` (with `Str8SplitFlags_SkipEmpty` to drop empty runs), `str8_list_push`, `str8_list_push_fmt`, `str8_join`, and `str8_list_to_array`.
  - transforms (arena-allocating): `str8_concat`, `str8_to_upper` / `str8_to_lower` (ASCII letters only), `str8_replace` (all non-overlapping occurrences).
  - parsing: `str8_to_u64` / `str8_to_i64` (strict: no whitespace, overflow rejected at `U64_MAX`/`I64_MAX`/`I64_MIN`) and `str8_to_f64` (interim `strtod` delegation; accepts its extended forms, rejects trailing junk and inputs ≥ 64 bytes).
- `str16` / `str16_view` types and `view_from_str16` / `view_from_raw` helpers.
- `file_write`: write a byte span to a path with create-or-truncate semantics, returning the byte count on success and `0` on failure (note: a successful zero-byte write also returns `0`). Backed by a now-implemented `os_file_open_for_write` and a chunked `os_file_write` that loops on partial `WriteFile` results.
- Internal `os_get_last_error` / `os_error_string` helpers (groundwork for richer file-error reporting).
- Tests: new `tests/test_files.c` (write/read round trip incl. embedded NUL, truncate-on-rewrite, failure paths, map round trip) and twelve new string cases covering every operation above.

### Changed
- **Breaking:** arena string helpers renamed so the result type leads the name: `arena_push_cstring` → `c_str_push_copy`, `arena_push_cstring_fmt` → `c_str_push_fmt`, `arena_push_str8_copy` → `str8_push_copy`, `arena_push_str8_from_cstring` → `str8_push_c_str`, `arena_push_str8_fmt` → `str8_push_fmt`.
- **Breaking:** file API renamed to a common `file_` prefix: `arena_read_file` → `file_read`, `map_file` → `file_map`, `unmap_file` → `file_unmap`.
- Arena-backed `str8` results are now NUL-terminated by convention (`size` still excludes the terminator; `str8_view` carries no such guarantee). `str8_push_copy` previously did not terminate.
- The implementation now includes `<stdlib.h>` and `<errno.h>` for the interim `strtod`-based `str8_to_f64`; both go away if/when it is hand-rolled.

## [0.0.11] - 2026-07-03

### Added
- `HighResTimer`: fixed-rate loop pacing via `high_res_timer_create`/`_wait`/`_release`.

### Changed
- **Breaking**: Arenas header now self hosted at the base of the reservation, and `arena_alloc` now returns a pointer. i.e., (```Arena a = arena_alloc(n);``` -> ```Arena* a = arena_alloc(n);```). `arena_release` no longer zeros arena struct and leaves a dangling pointer (similar to `free`).

## [0.0.10] - 2026-06-30

### Changed
- **Breaking:** `ring_buffer_alloc` now returns a `RingBuffer` by value and **panics** (`FATAL`) on any failure path, mirroring `arena_alloc`. It was `b8 ring_buffer_alloc(RingBuffer*, u64)` returning `false` on failure. Update callers from `if (!ring_buffer_alloc(&rb, n)) {…}` to `RingBuffer rb = ring_buffer_alloc(n);`. On pre-1803 Windows (backing entry points unavailable) this now panics instead of returning `false` — gate the call if you need to degrade gracefully.

## [0.0.9] - 2026-06-30

### Added
- `str8_has_prefix` / `str8_cut` / `view_from_c_str`: prefix test; split-once on a separator (fills `before`/`after` views and returns `b8` found/not-found); and a `str8_view` over a NUL-terminated C string. Covered in `tests/test_strings.c`.
- Numeric-limit macros `I8_MIN`…`I64_MIN`, `I8_MAX`…`I64_MAX`, `U8_MAX`…`U64_MAX`, with `AETHER_NO_NUMERIC_LIMITS` to opt out (the public names defer to any pre-existing definition; the values are always available as private `AETHER_*_` names). These replace the limits previously reached for via `<stdint.h>`/`<limits.h>`.
- `b32`: 32-bit boolean alias (over `u32`), alongside the existing `b8`.

### Changed
- The header is now standard-conformant **C11 / C++11** instead of relying on compiler extensions: C99 compound-literal returns became named temporaries (identical `-O2` codegen), and `STR` expands through a new `AETHER_LITERAL(T)` helper (`(T)` in C, `T{}` in C++). Previously the header compiled as C++ only via extensions (designated initializers and the `(T){…}` compound-literal syntax). Conformance is now gated by pedantic C11 / C++11 compile probes in the test suite.
- Dropped the `<stdint.h>` and `<stdbool.h>` includes. Fixed-width aliases (`i8`…`u64`, `f32`/`f64`) are now plain typedefs over the built-in types, each guarded by a `_Static_assert` on its `sizeof`, so a platform whose type widths differ fails to compile rather than silently misbehaving. `b8` is now an alias for `u8` (was `bool`).
- **Breaking:** the `DEBUG_BREAK()` macro is renamed `AETHER_DEBUG_BREAK()` to keep aether's macro namespace prefixed. Consumers using `DEBUG_BREAK` directly must update.

### Fixed
- `arena_push_array` / `arena_push_array_zero` / `arena_push_array_nozero` now reject a `count * sizeof(T)` product that would overflow `u64`. The size is computed in a checked `static inline` helper (`arena_push_array_`) rather than inline in the macro, preventing a silently-undersized allocation.

## [0.0.8] - 2026-06-26

### Added
- `ring_buffer_available`: returns the number of unread bytes (`write - read`) in the buffer, or `0` for a `NULL`/unallocated buffer.
- `ring_buffer_advance_read`: advances the read cursor by `len` without copying, rejecting an advance past the written data. Enables a zero-copy consume — `ring_buffer_peek` to get a contiguous view, use it in place, then `ring_buffer_advance_read` to release exactly what was consumed.

### Changed
- **Breaking:** `c_str` takes the arena first now — `c_str(Arena*, str8_view)` — for consistency with `arena_push_*`. Empty/`{NULL,0}` input returns an arena-allocated `""` rather than a string literal, keeping the result's ownership and writability uniform.
- `ring_buffer_read` is now implemented as `ring_buffer_peek` + `memcpy` + `ring_buffer_advance_read`. Behavior is unchanged; the copy and zero-copy paths now share one definition of what is readable.

### Fixed
- `ring_buffer_alloc` now casts base ptr to `u8*`, fixing cpp compile error


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
