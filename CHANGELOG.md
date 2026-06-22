# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
