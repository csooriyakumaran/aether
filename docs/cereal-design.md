# Design: CEREAL — Minimal RS-232 Serial for C/C++

Status: DRAFT (2026-08-19).
Consumer driving the requirements: a pressure-transducer datalogger — one
instrument, one port, ASCII query/reply over RS-232, scheduled polling,
log to file. No second consumer yet. Per `iris`'s own precedent (its
design doc: "keep it here until a second project needs it"), this can
live as a single header inside the datalogger app first; promotion to a
standalone module alongside `aether`/`iris` is a copy-paste-rename if a
second consumer shows up, not a decision made now.

This doc deliberately starts where `iris-networking-design.md` *ended up*
(blocking calls, no poll) rather than retracing iris's non-blocking
detour — the reasoning for skipping straight there is decision 2 below.

## Motivation

The datalogger issues a short ASCII command (`"P?\r\n"`), waits for the
instrument's reply (`"1013.25\r\n"`), and logs it, on a fixed schedule.
One port, one instrument, no concurrent I/O. The library's whole job is:
open the port with the right line settings, write the query, read the
reply within a bounded deadline, close on shutdown. Everything about
*what* the query means, *when* to issue it, and *where* the reply gets
logged is the application's problem, not this header's.

## Requirements

| # | requirement |
|---|-------------|
| R1 | Open a named serial device (`COM3`, `/dev/ttyUSB0`) with baud/format config; one persistent connection, no listen/accept/reconnect state machine |
| R2 | Blocking write of a command buffer; partial-count out-param (short writes are possible, not an error) |
| R3 | Blocking read of the reply, bounded by a deadline configured once at open — never blocks past it, no non-blocking mode, no poll |
| R4 | Caller can distinguish "no reply in time" (retry) from "device unplugged" (stop, alert) from "other error" (log) — three outcomes handled differently |
| R5 | No hidden global state; explicit open/close per port |
| R6 | Minimal dependencies; C11; single header |
| R7 | Byte-level only — command text and the ASCII framing (`\r\n` terminators) are the caller's job |

## Scope

Open/configure, blocking write, blocking read-with-deadline, close. Thin
wrapper over `CreateFile`+DCB/`COMMTIMEOUTS` (Windows) and `termios`
(POSIX) — same category as iris wrapping sockets.

**Non-goals:**
- Port discovery/enumeration (`SetupAPI`, `/sys/class/tty`). Caller
  supplies the device path; it already knows which port the instrument is
  on.
- Reconnect-on-unplug logic. `SerialResult_Disconnected` is reported; what
  to do about it is the caller's policy.
- Multi-drop / addressed protocols (Modbus and friends). Out of scope
  entirely — this is a point-to-point ASCII link.
- Non-blocking mode, `serial_poll`, cross-thread cancellation. R3 makes
  every call bounded by construction; there is nothing indefinite here for
  a poll or a cancel-by-close trick to interrupt (contrast with why iris
  needed them before it had `Thread`).

**Deferred until there's a caller:** write-side timeout (see decision 6),
7-bit/parity variants beyond what `SerialConfig` already exposes,
multi-port management.

## Decisions and rationale

1. **`SerialPort` is a `u64` handle, `os_value + 1`, `{0}` = invalid.**
   Same convention as `Socket`. Needed on POSIX (`open()` can return fd
   `0`); on Windows a valid `HANDLE` is never literally `0` so the bias is
   redundant there, but one rule across both platforms beats a
   platform-conditional invalid sentinel.

2. **Every call blocks; there is no non-blocking mode, and there never
   was one to remove.** Sockets needed non-blocking+poll (then
   thread+cancellation) because `accept`/`recv` block indefinitely with no
   natural deadline. A transducer's reply time is bounded by the
   instrument itself — R3's deadline is a property of the link, not a
   workaround. Baking it into the OS's own timeout mechanism
   (`COMMTIMEOUTS.ReadTotalTimeoutConstant` / `termios` `VMIN=0,VTIME=`)
   means `serial_read` already returns on its own; there's nothing left
   for a poll loop or a cancellation contract to do.

3. **No dedicated-thread / cross-thread-cancellation machinery (no
   analogue to iris decision 11).** One port, one owning thread. Shutdown
   is: stop issuing new reads, let the in-flight bounded read return on
   its own (worst case, one deadline period), then close. This is
   simpler than iris's `socket_close`-cancels-a-blocked-call contract, and
   deliberately so — that contract is Windows-only-guaranteed even for
   sockets (iris's own portability note), and `CloseHandle`/`close()` on a
   handle another thread is synchronously blocked on has no such guarantee
   for serial reads at all. Don't reach for it; the bounded-read design
   doesn't need it. Revisit only if a real caller needs the scheduler
   decoupled onto its own thread — then design the cancellation contract
   for that case specifically, don't inherit iris's.

4. **Four-state result, `Timeout` first-class and distinct from
   `Disconnected`/`Error`.**

   ```c
   typedef u8 SerialResult;
   enum SerialResult_
   {
       SerialResult_Ok = 0,
       SerialResult_Timeout,       /* no reply within the deadline — routine, retry */
       SerialResult_Disconnected,  /* device gone: ENXIO/EIO, ERROR_DEVICE_REMOVED */
       SerialResult_Error,         /* anything else */
   };
   ```

   Iris collapsed to three states because every non-`Ok` socket outcome
   is handled identically (log, drop). Here the three failure outcomes
   are handled *differently* by the scheduler (retry next tick / stop and
   alert the operator / log and continue) — collapsing them would just
   force the caller to re-derive the distinction from `errno` at the call
   site, which is exactly what iris's own result-model rationale argues
   against.

5. **The read deadline is configured once, in `SerialConfig`, not passed
   per call.** It's a property of the instrument/link (how long this
   transducer takes to answer), not of an individual query, and both
   `COMMTIMEOUTS` and `termios` are port-wide settings, not per-call ones
   — this follows the OS's own shape rather than fighting it with a
   per-call parameter the underlying API can't honor per-call anyway.

6. **Write has no timeout in v1.** `termios`/`COMMTIMEOUTS` bound reads,
   not writes; a write only stalls if hardware flow control is enabled
   and the instrument never asserts CTS. No flow control is the common
   case for a simple query/reply transducer. Noted as a known gap
   (deferred, not solved) rather than adding a knob with no caller.

7. **Device path is an opaque `str8_view`, OS-native syntax, no
   normalization.** `"COM3"` vs. `"\\.\COM10"` (ports ≥10 need the prefix
   on Windows) vs. `"/dev/ttyUSB0"` — caller supplies the right string for
   its platform. No cross-platform path table, no enumeration. Same
   deferral iris made for DNS: literal address in, no resolution.

8. **Framing is entirely the caller's job.** `serial_write`/`serial_read`
   move bytes; `\r\n` command/reply parsing lives in the datalogger, same
   split iris draws between byte-stream `tcp_recv` and line/record
   parsing.

9. **Platform ifdefs confined to the `os_` layer, noun-verb naming** —
   inherited convention from aether/iris, not a new decision.

## Public API (cereal.h)

```c
typedef struct SerialPort { u64 handle; } SerialPort;   /* {0} = invalid */

typedef u8 Parity;
enum Parity_ { Parity_None, Parity_Even, Parity_Odd, Parity_Mark, Parity_Space };

typedef u8 StopBits;
enum StopBits_ { StopBits_One, StopBits_Two };

typedef u8 FlowControl;
enum FlowControl_ { FlowControl_None, FlowControl_RtsCts };

typedef struct SerialConfig
{
    u32         baud;
    u8          data_bits;       /* 7 or 8 */
    Parity      parity;
    StopBits    stop_bits;
    FlowControl flow;
    u32         read_timeout_ms; /* query/response deadline, see decision 5 */
} SerialConfig;

typedef u8 SerialResult;
enum SerialResult_
{
    SerialResult_Ok = 0,
    SerialResult_Timeout,
    SerialResult_Disconnected,
    SerialResult_Error,
};

CEREAL_API SerialPort   serial_open(str8_view device, SerialConfig cfg); /* {0} on failure */
CEREAL_API void         serial_close(SerialPort* p);                    /* idempotent */
CEREAL_API SerialResult serial_write(SerialPort p, bytes_view data, u64* out_sent);
CEREAL_API SerialResult serial_read(SerialPort p, void* buf, u64 cap, u64* out_read);
```

`bytes_view`/`str8_view`/`u8`/`u32`/`u64` from `aether.h`. `CEREAL_API`
mirrors `IRIS_API`/`AETHER_API` (static/DLL linkage defines, same
`*_IMPLEMENTATION` single-header pattern).

## Internal `os_` surface

```c
internal u64          os_serial_open(str8_view device, SerialConfig cfg);
internal void          os_serial_close(u64 h);
internal SerialResult os_serial_write(u64 h, const u8* data, u64 len, u64* out_sent);
internal SerialResult os_serial_read(u64 h, u8* buf, u64 cap, u64* out_read);
```

| os_ helper | Windows | POSIX |
|---|---|---|
| `os_serial_open` | `CreateFileW` + `SetCommState` (DCB) + `SetCommTimeouts` | `open(O_RDWR \| O_NOCTTY)` + `cfmakeraw` + `cfsetspeed` + `c_cflag` (CS7/CS8, PARENB/PARODD, CSTOPB) + `VMIN=0,VTIME=<deadline/100>` + `tcsetattr` |
| `os_serial_write` | `WriteFile` | `write` |
| `os_serial_read` | `ReadFile` (timeout via `COMMTIMEOUTS` set at open) | `read` (timeout via `VTIME` set at open) |
| `os_serial_close` | `CloseHandle` | `close` |

No dynamic loading (contrast iris's self-hosted `ws2_32`): `kernel32`
(Windows) and libc (POSIX) are already linked by every consumer, so
`CreateFileW`/`ReadFile`/etc. and `open`/`termios`/etc. are called
directly — no `GetProcAddress` resolution needed.

## Usage sketch — scheduler loop (single-threaded, no coordination needed)

```c
SerialConfig cfg = { .baud = 9600, .data_bits = 8, .parity = Parity_None,
                      .stop_bits = StopBits_One, .flow = FlowControl_None,
                      .read_timeout_ms = 200 };
SerialPort port = serial_open(str8_view_from_cstr("COM3"), cfg);

for (;;)
{
    if (shutdown_requested) break;           /* checked once per tick, see decision 3 */

    bytes_view query = str8_as_bytes(str8_lit("P?\r\n"));
    u64 sent = 0;
    if (serial_write(port, query, &sent) != SerialResult_Ok) { /* log, continue */ }

    u8 buf[64]; u64 got = 0;
    SerialResult r = serial_read(port, buf, sizeof(buf), &got);
    switch (r)
    {
        case SerialResult_Ok:           /* parse ASCII reply, append to log file */ break;
        case SerialResult_Timeout:      /* count a missed sample, retry next tick */ break;
        case SerialResult_Disconnected: /* alert operator, break loop */            break;
        case SerialResult_Error:        /* log, retry next tick */                  break;
    }

    timer_sleep_until(next_tick);            /* aether HighResTimer, not cereal's concern */
}
serial_close(&port);
```

## Pitfalls to preserve when implementing

- **Windows COM ports ≥10 need the `\\.\COM10` prefix** — plain `"COM10"`
  fails to open; `"COM3"` (single digit) works either way. Document at
  `serial_open`, don't silently rewrite the caller's string.
- **`cfmakeraw` before setting any other `termios` flag.** Canonical mode
  (the POSIX default) line-buffers and can strip/echo bytes; every other
  flag set on top of raw mode, not the reverse order.
- **`read_timeout_ms` must exceed worst-case instrument response time**,
  or every query reports `Timeout` even on a healthy link. This is a
  config/calibration concern for the app, not something the library can
  validate.
- **Partial write/read counts still matter** even though ASCII commands
  are short — don't assume one `serial_write`/`serial_read` call moves the
  whole buffer; honor `out_sent`/`out_read` the same way iris's callers
  honor `out_sent` for TCP.
- **Do not copy iris's close-cancels-a-blocked-call trick.** Decision 3:
  no code path here should assume `CloseHandle`/`close()` from another
  thread safely interrupts a pending synchronous read.

## Validation plan

No loopback-over-localhost equivalent for serial; use a virtual null-modem
pair (`com0com` on Windows, a `socat PTY,link=... PTY,link=...` pair on
POSIX) as the test harness, with a small mock instrument on one end that
answers deterministically (including a deliberately-silent mode to
exercise `Timeout`).

1. **Lifecycle:** open/close round-trip; double-close on `{0}` is a no-op;
   read/write on an invalid port fail cleanly.
2. **Round trip:** write a query on one end of the pair, read it on the
   mock, write a reply back, verify bytes and `out_sent`/`out_read` match.
3. **Timeout:** mock end silent; assert `serial_read` returns
   `SerialResult_Timeout` within (not after) `read_timeout_ms` plus a
   small tolerance, not before.
4. **Disconnected:** close the mock's end of the pair (or, on real
   hardware, physically unplug a USB-serial adapter mid-test) and assert
   `SerialResult_Disconnected`, not a generic `Error`.
5. **Partial I/O:** a buffer capacity smaller than the reply forces a
   short `serial_read`; verify the caller's loop (re-read for the
   remainder) reassembles the full reply correctly.

## Follow-ups after landing

- Wire into the datalogger app as `cereal.h` directly (no promotion yet —
  single consumer, per the top of this doc).
- If a second consumer appears: promote alongside `aether`/`iris` as its
  own single-header module, `CEREAL_API`/`CEREAL_STATIC`/`CEREAL_DLL`
  linkage defines mirroring the existing pattern.
- Port discovery/enumeration, reconnect policy, and multi-port management
  stay explicitly out until a caller needs them (Scope, non-goals).
