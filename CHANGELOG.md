# Changelog
All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed
- `enum`s are now all `typedef`'d to fixed width integers
- ***breaking** `local_persist` alias for `static` used in function bodies renamed -> `persist`.


## [0.0.17] - 2026-08-18

### Added
- `str8_to_int(s, min, max, out)`: bounded-range signed integer parsing — delegates to `str8_to_i64` for the actual digit parsing/overflow handling, then rejects the result if it falls outside `[min, max]`. Added because call sites kept re-deriving the same "parse as `u64`, range-check, downcast" pattern by hand (e.g. `iris`'s octet and port parsing).
- `str8_to_u8` / `str8_to_u16` / `str8_to_u32` and `str8_to_i8` / `str8_to_i16` / `str8_to_i32`: thin typed wrappers over `str8_to_int` with fixed bounds (`AETHER_U8_MAX_`, `AETHER_I16_MIN_`/`MAX_`, etc.), mirroring the `arena_alloc` / `arena_alloc_ex` convention — general bounded core, narrow convenience wrappers on top. `str8_to_u64` and `str8_to_i64` are unchanged and stay standalone: `U64_MAX` doesn't fit in the `i64` range `str8_to_int` works in, so the 64-bit parsers can't be expressed as wrappers over it.
- `str8_to_u64` / `str8_to_i64` gain `0x` / `0o` / `0b`-prefixed integer literal parsing (case-insensitive prefix and hex digits) alongside plain decimal, sharing the existing overflow-safe digit accumulation across all bases. Sign and base prefix compose correctly (`"-0x8000000000000000"` parses to `I64_MIN`); a bare leading zero with no prefix (`"007"`) is still decimal, not C-style implicit octal.
- `Str8ListForEach(list, node)`: for-each macro over `Str8List`, declaring a loop-scoped `Str8Node* node` cursor and walking `node->next` until `NULL`, so call sites don't hand-roll the same linked-list walk.
- `Str8SplitFlags_Trim`: new `str8_split` flag that runs each token through the existing `str8_trim` before the `Str8SplitFlags_SkipEmpty` check, so a whitespace-only token counts as empty under `Trim | SkipEmpty`. Since `Str8List` nodes are `str8_view`s into the source buffer, trimming is pointer/length adjustment only — no allocation, no extra pass over the list. Motivated by delimiter-only CSV parsing (`,` as the sole separator, with the conventional space after each comma); quoted fields with embedded commas/newlines are out of scope for `str8_split` and would need a dedicated parser.
- `str8_cut_ex(s, sep, before, after, flags)`: generalized `str8_cut`, taking a new `Str8CutFlags` (`Trim`, `Last`). `Trim` runs both `before`/`after` through `str8_trim`, applied uniformly whether or not `sep` was found. `Last` cuts at the final occurrence of `sep` instead of the first (e.g. splitting a path on the last `/` for dirname/basename), delegating to the existing `str8_find`/`str8_find_last` instead of a third hand-rolled scan loop. `str8_cut` is now a thin `Str8CutFlags_None` wrapper over `str8_cut_ex`, mirroring the `arena_alloc`/`arena_alloc_ex` convention — all existing `str8_cut` call sites are unaffected. An explicit `sep.size == 0` guard ahead of the `str8_find*` calls preserves `str8_cut`'s original contract (empty separator never matches) despite `str8_find`/`str8_find_last` treating an empty needle as an always-match at position `0`/`s.size`.
- `datetime` (wall-clock calendar type — `year`/`month`/`day`/`hour`/`minute`/`second` fields plus nanosecond-resolution `ns`) and its conversions:
  - `wall_clock_ns()`: raw nanoseconds since the Unix epoch, sourced from `GetSystemTimePreciseAsFileTime` (Win8+, same or lower OS-version bar than other aether APIs already require) rather than the coarser `GetSystemTimeAsFileTime` — this is the actual wall-clock precision ceiling on Windows (100ns ticks), not a true 1ns source, though the API stores full nanosecond resolution.
  - `datetime_now()`: `wall_clock_ns()` decomposed into calendar fields via `datetime_from_ns_since_epoch()` — no separate OS call for the breakdown.
  - `datetime_from_ns_since_epoch(ns)` / `datetime_to_ns_since_epoch(dt, out)`: symmetric encode/decode built on Howard Hinnant's `days_from_civil`/`civil_from_days` proleptic-Gregorian algorithms (`days_from_civil_`, `civil_from_days_`, `is_leap_year_`, `days_in_month_` — all private). The encode direction validates every field (month/day-for-month/hour/minute/second/ns range, rejects pre-1970 dates and far-future overflow past what a `u64` nanosecond count can hold, roughly year 2554) rather than trusting the input, since `datetime` is a plain public struct that can be hand-built from untrusted data, not just produced by `datetime_now()`.
- `Utf8Decode utf8_decode(str8_view s)`: decodes one UTF-8 codepoint at `s.data[0]`, returning `{codepoint, len}` where `len` (bytes consumed, 1-4) is `0` for any invalid input — truncated sequence, invalid lead byte, stray continuation byte, overlong encoding, surrogate half (`U+D800`-`U+DFFF`), or a codepoint past `U+10FFFF`. `str8`/`str8_view` themselves stay plain bytes; this is an opt-in decode step for callers (e.g. a terminal layer) that need to walk codepoint boundaries.
- `u8 utf8_codepoint_width(u32 codepoint)`: terminal column width classification — `0` (control chars, combining marks, zero-width joiners/spaces, variation selectors), `1` (everything else), or `2` (CJK/Hangul/Kana/fullwidth/emoji blocks). Not a full UAX #11 implementation, just the ranges a terminal layer needs to get column math right for real-world text; private range tables (`utf8_zero_width_ranges_`, `utf8_wide_ranges_`) are a deliberately non-exhaustive, hand-picked subset.

### Fixed
- iris: `net_addr_parse` / `net_addr_parse_hostport` now reject `0x`/`0o`/`0b`-prefixed octets and ports. `str8_to_u64`'s new multi-base literal support (above) would otherwise let e.g. `"0x1.0x2.0x3.0x4"` parse as a valid address and `"192.168.0.1:0x50"` parse as port 80 — outside the "numeric only" (decimal) contract `net_addr_parse` documents. Guarded by a new private `str8_is_decimal_` pre-check ahead of the `str8_to_u64` call; plain leading-zero decimal (`"010"`, `"080"`) is unaffected.

## [0.0.16] - 2026-08-09

### Added
- `HighResTimer` now tracks the actual wake instant: a new `wake_time` field is set on every `high_res_timer_wait` call — to `now` on the missed-deadline path (exact, since no sleep/spin happens there) and to the last spin-sampled timestamp on the normal path. Callers get the pacing timestamp for free instead of taking a second, separate `time_mark()` after `wait()` returns.
- iris: `net_addr_parse_hostport(s, default_port, out)` parses `"ip"`, `"ip:port"`, `"localhost"`, or `"localhost:port"` (case-insensitive), delegating numeric-IP validation to the existing `net_addr_parse` and using `default_port` only when the string doesn't specify one. `net_addr_parse` itself is unchanged — still strictly numeric, no DNS.
- iris: `net_addr_to_cstr(addr, buf, buf_size)` formats a `NetAddr` as `"a.b.c.d:port"` into a caller-owned buffer, no allocation; `net_addr_to_str8(arena, addr)` is an arena-backed convenience wrapper around it. New `NET_ADDR_STR_MAX` (22) sizes the longest possible formatted address.

### Fixed
- `high_res_timer_alloc`: `spin_margin` is now clamped to at most half the timer's period (`AETHER_MIN_(1ms, period_ticks / 2)`) instead of a fixed 1ms. At high tick rates (>1kHz) the old fixed margin could exceed the whole period, starving `os_timer_sleep` of a positive duration and forcing every wait into a full-period busy spin.

## [0.0.15] - 2026-08-07

### Fixed
- **iris** headers were never installed: the top-level `install(DIRECTORY ...)` rule only covered `include/aether`, so `cmake --install` (and the release script's zip built from it) silently omitted `include/iris/iris.h`. Present in the `0.0.14` release archive as a missing header — a mirrored `install(DIRECTORY include/iris ...)` rule fixes it.

## [0.0.14] - 2026-08-07

### Added
- **iris**: new networking library (`include\iris\iris.h`), Windows-only for now (POSIX branches are labeledas `#error` stubs). Process-wide lifetime via `net_init`/`net_shutdown`, which pin and dynamically load `ws2_32.dll` at runtime (`LoadLibraryExW` + `GetProcAddress`) - no `-lws2_32` link dependency, mirroring aether's `VirtualAlloc2` runtime-loading pattern. Address construction: `NetAddr` (IPv4, wire-order `ip[4]` + host-order `port`), `net_addr` / `net_addr_any` / `net_addr_loopback`, `net_addr_parse` (numeric dotted-quad only, no DNS). `Socket` is a `{ u64 handle }` with a `+1` bias so `{0}` is invalid, matching aether's zero-init-is-empty convention.
- iris TCP: `tcp_listen`/`tcp_accept`/`tcp_connect`/`tcp_send`/`tcp_recv`. Every call blocks - there is no non-blocking mode and no readiness-polling API; a socket is cancelled from another thread by calling `socket_close` on it, which Windows guarantees unblocks any pending call. `TCP_NODELAY` is always set. Partial sends are reported through an `out_sent` count rather than looped-until-complete internally, so zero-copy send paths (`ring_buffer_peek` -> `tcp_send` -> `ring_buffer_advance_read(sent)`) get honest accounting. `NetResult` is a three-state result (`OK`/`Closed`/`Error`) plus an out-count. 
- iris UDP: `udp_open`/`udp_send_to`/`udp_recv_from`. Same blocking, self-hosted-`ws2_32` shape as TCP, minus the TCP-only setup (no `SO_EXCLUSIVEADDRUSE`, `listen`, or `TCP_NODELAY`). Datagrams send whole or not at all — an oversized send or a short `sendto` both surface as `NetResult_Error`, never a partial count. Unlike `tcp_recv`, a zero-byte `recvfrom` is a legitimate empty datagram, not `NetResult_Closed` — UDP has no connection state to close.
- `HighResTimer` construction is split so its clock doesn't have to start ticking the moment the resource exists: `high_res_timer_alloc(hz)` allocates the OS timer and sets the tick rate but leaves the clock inert; `high_res_timer_arm(&t)` starts (or restarts) it, syncing the deadline to now, clearing `overrun`, and returning the start-of-clock timestamp (comparable to `time_mark()`) so callers can track it without reaching into `HighResTimer.next_deadline`. `high_res_timer_create(hz)` remains as `alloc`+`arm` convenience for the common immediate-start case. `HighResTimer` gains an `armed` field; `high_res_timer_wait` on a never-armed timer is now a defined, safe no-op (returns `0`) rather than risking a `period_ticks == 0` divide or a garbage-deadline overrun burst if called before the timer is configured. `high_res_timer_set_rate` now only updates `period_ticks` and no longer resyncs the deadline to "now", so changing the tick rate mid-loop doesn't reset phase — call `high_res_timer_arm` again to explicitly restart.
- Console signal handling: `console_signal_install`/`console_signal_uninstall` wrap `SetConsoleCtrlHandler` so application code never touches it directly. Covers Ctrl+C, Ctrl+Break, console-close, logoff, and shutdown as one generic notification (no per-event dispatch, no `DWORD ctrl_type` leaking into the public API); only one handler may be installed at a time. `fn` runs on an OS-created thread that is neither the calling thread nor any aether `Thread`, so it should stay tiny (flag-set only). `fn`/`user` are published to that thread through a dedicated atomic gate (`console_signal_ready_`, `atomic_store_rel_u64`/`atomic_load_acq_u64`) rather than relying on `SetConsoleCtrlHandler`'s kernel transition as an implicit memory barrier — the latter isn't a synchronization primitive the C11/C++11 memory model recognizes, even though it would likely work in practice. These are the first file-scope mutable globals in `aether.h` (everything else is stack-local or per-call state); named with the header's existing `global` macro rather than `internal` to keep that distinction meaningful now that it's actually used. Covered by `tests/test_console_signal.c`'s `install_contract` case (double-install rejection, uninstall idempotency, gate reopening); a `fires_on_ctrl_c` case exercising a real `GenerateConsoleCtrlEvent` also exists but isn't wired into the automated suite — it needs a normal interactive console session to deliver at all.
- `atomic_cas_u64` - added atomic compare and swap. returns true if swap occurred, false if *p no longer matches expected value (in which case *p is untouched)
- `ring_buffer_reserve` / `ring_buffer_commit` / `ring_buffer_cancel_reservation`: reserve-fill-publish path for `RingBuffer`, so a caller can get a pointer to the next `len` bytes, fill it in piecewise (out of order, in parts), and publish with one `commit` instead of staging into a temporary buffer first. `reserve` fails (`{0}`) while a reservation is already outstanding — only one in-flight reservation is supported, matching the single-producer contract. `commit(len)` may publish `len <= reserved` bytes (reserve worst-case, commit actual size used) and is rejected if `len` exceeds what was reserved; `cancel_reservation` drops a reservation without publishing. `ring_buffer_write` is now implemented in terms of `reserve` + `commit` rather than duplicating the free-space check. Covered by new cases in `tests/test_ring.c` (roundtrip, wrap seam, misuse guards, cancel, partial commit, and a regression for a zero-length `write()` clobbering an outstanding reservation).

### Changed
- **Breaking:** `RingBuffer`'s fields are reordered and cache-line padded: `read` (mutated only by the consumer) is isolated onto its own 64-byte-aligned cache line, separate from `write`/`reserved`/`base`/`size` (mutated only by the producer, or read-only after `alloc`). This avoids false-sharing cache-coherency traffic between the producer and consumer threads on every push/pop. `sizeof(RingBuffer)` grows from 32 to 128 bytes; code that depends on field order (positional initialization, `offsetof` assumptions) breaks — field-name access (`rb.read`, `rb.write`, ...) is unaffected. New public `AETHER_ALIGN(n)` macro (`__declspec(align)` / `__attribute__((aligned))`) and `AETHER_CACHE_LINE_SIZE` (64) back the layout.

## [0.0.13] - 2026-07-14

### Added
- Context detection section: `AETHER_COMPILER_MSVC/GCC/CLANG`, `AETHER_OS_WINDOWS/MAC/LINUX/ANDROID/BSD` (+ derived `AETHER_OS_POSIX`), `AETHER_ARCH_X64/ARM64/X86`, `AETHER_LANG_C/CPP` (+ `AETHER_LANG_C23`), and `AETHER_BUILD_DEBUG`. All are always defined to `0` or `1` so use sites are `#if` (never `#ifdef`); an undetected compiler/OS/arch fails with a labeled `#error`, as do targets outside the current support envelope (Windows, 64-bit x64/arm64).
- `AETHER_API` linkage control, documented in the header preamble. Default is `extern` with `AETHER_IMPLEMENTATION` in exactly one TU (unchanged); `AETHER_STATIC` makes the whole API private to the implementing TU; `AETHER_BUILD_DLL` marks it `dllexport` (`visibility("default")` on POSIX, for later) for shared-library builds; `AETHER_DLL` marks it `dllimport` for consumers. Conflicting combinations are compile errors.
- Atomics (new public section): `atomic_load_acq_u64` / `atomic_store_rel_u64` — acquire/release semantics, static inline in the header. Supported pairs are decided once by an internal dispatch (`AETHER_ATOMICS_GNU` for GCC/Clang on any arch via `__atomic_*`; `AETHER_ATOMICS_MSVC` for MSVC on x64/arm64 via `__iso_volatile_*` plus a per-arch barrier — compiler-only `_ReadWriteBarrier` on x64 (TSO), `dmb ish` on arm64); anything else fails with a labeled `#error` naming the pair. 64-bit targets enforced at compile time.
- Threads (new public section): `Thread`, `thread_fn`, `ThreadPriority`, with `thread_create` / `thread_join` / `thread_set_priority` / `thread_yield` / `thread_sleep_ms`. Windows-only for now — non-Windows paths are labeled `#error` stubs pending the POSIX port. `thread_create` uses `_beginthreadex` (CRT-safe, unlike raw `CreateThread`; the implementation now includes `<process.h>` for it) and hands the start arguments off through an acquire/release handshake, so they live on the caller's stack and creation does not return until the new thread has copied them. `thread_join` waits, surfaces the exit code through an optional out-param, closes and nulls the handle, and returns `b8` (`false` for a `NULL`/already-joined thread — joining twice is safe, not UB). `thread_set_priority` maps `Normal`/`High`/`TimeCritical` onto the Win32 priorities and returns `b8` success. `thread_sleep_ms` uses a high-resolution waitable timer (`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, Windows 10 1803+) and degrades to plain `Sleep` on older systems; `thread_sleep_ms(0)` yields the timeslice. Covered by `tests/test_ring_spsc.c`, which drives the SPSC ring buffer from real producer/consumer threads through this API (including priority and sleep paths).

### Changed
- Arena-allocating string functions (`str8_push_*`, `c_str*`, `str8_concat` / `str8_join` / `str8_replace` / `str8_to_upper` / `str8_to_lower`, list helpers) now treat arena exhaustion as fatal (`FATAL`), consistent with the reserve/commit failure policy. Previously an exhausted arena meant an unchecked `NULL` write. Also silences GCC `-Wstringop-overflow` in release builds. `arena_push` itself still returns `NULL` on overflow for callers that can recover (e.g. `file_read`).
- `ring_buffer_read` with `len == 0` now returns `true` (reading nothing succeeds); previously it returned `false`, conflating "asked for nothing" with "not enough data". `ring_buffer_write` and `ring_buffer_advance_read` already treated zero as a no-op success.
- Internal `os_get_last_error` / `os_error_string` helpers (added in 0.0.12, never called) are disabled until they gain a caller; they produced unused-function warnings in every implementation TU under clang `-Wall` and MSVC C++ `/W4`.
- `view_from_raw` moved from the strings section to the bytes section and is now declared returning `bytes_view` (was `str8_view`). It takes untyped memory and makes no encoding or NUL-termination assumptions, so it belongs with the bytes constructors alongside `view_from_bytes`. Not breaking: `str8_view` is a typedef of `bytes_view`, so every existing call site compiles unchanged.

### Removed
- `bytes_view_from_str8_view` (added post-0.0.12, never in a release): with `str8_view` aliasing `bytes_view` it was an identity function on a single type. Callers can pass a `str8_view` anywhere a `bytes_view` is expected directly.

### Fixed
- `RingBuffer` is now actually safe for its documented single-producer/single-consumer use: the `read`/`write` counters are published with acquire/release atomics. Previously they were plain `u64` accesses, so cross-thread use was a data race - the write index could become visible before the bytes it covers, or a region could be reused before the consumer finished reading it. Contract now explicit: exactly one producer thread and one consumer thread; a `ring_buffer_peek` view is invalid after the matching `ring_buffer_advance_read`. Covered by a new two-thread stress case (`spsc_stress`) in `tests/test_ring.c`.
- `arena_push` bounds check hardened against alignment overflow: an alignment that pushed the aligned position past the reservation (or wrapped `u64`) slipped past the old check in release builds via unsigned underflow, yielding an out-of-bounds pointer (access violation or a misleading `FATAL("Memory commit failed")`). Such pushes now return `NULL` like any other overflow; verified by hostile-alignment tests in debug and release.
- `file_read` now handles arena exhaustion explicitly: on a failed push it closes the file handle, unwinds the arena to its mark, and returns an empty result. Previously it relied on `ReadFile` rejecting a `NULL` buffer and leaked nothing only by accident.
- String functions no longer pass `NULL` to `memcpy`/`memcmp` when handed empty views (`str8_push_copy`, `str8_concat`, `str8_eq`, `str8_cmp`): pedantic UB before C2y, and flagged by UBSan.
- Implementation TUs compile warning-free again under MSVC `/W4` (debug and release) and clang `-Wall -Wextra`: assert-only results are voided when asserts are compiled out, and `BOOL`-to-`b8` conversions are normalized.

## [0.0.12] - 2026-07-10

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
