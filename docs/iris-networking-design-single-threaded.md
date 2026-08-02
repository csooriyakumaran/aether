# Design: IRIS Minimal Networking (sockets) — single-threaded, polling variant

Status: SUPERSEDED (2026-08-02) by `iris-networking-design.md`. Kept for
history and for the rationale behind decisions that carried over unchanged
(address model, result-model shape, `TCP_NODELAY`, ws2_32 loading, etc.).
The non-blocking + `net_poll` mechanism this doc centers on (decisions 5,
9, 10) was abandoned once `aether` grew a real `Thread` API — a dedicated
network thread can now be cancelled by another thread instead of polling a
shared quit flag every tick. Do not implement against this doc.

Status: DRAFT (2026-07-14).
Consumer driving the requirements: `mps-emulator-c` (`docs/design/00-architecture.md`,
`01-concurrency.md`, `02-device.md` in that repo). Everything that repo's
network thread needs, and nothing it does not.
Prerequisites already landed in aether: atomics, threads, the SPSC-correct
`RingBuffer`, `HighResTimer`, and the `AETHER_API` linkage-mode pattern that
`IRIS_API` mirrors.

## Motivation

The MPS4200 emulator runs one **network thread** that owns every socket and
moves bytes between the wire and three SPSC ring channels (cmd / rsp / data):

```
        TCP :23  command lines in, replies out     TCP or UDP data frames out
             │            ▲                                 ▲
             ▼            │                                 │
        ┌──────────────────────────────────────────────────────┐
        │                   network thread                     │
        │  poll (bounded ~50 ms) -> accept / recv / send       │
        │  never blocks unboundedly: must observe quit flag    │
        └──────────────────────────────────────────────────────┘
```

Its needs are narrow: a TCP listener with a single command client, an
optional UDP data target, readiness polling with a bounded timeout, and
honest partial-send reporting so the zero-copy
`ring_buffer_peek -> send -> ring_buffer_advance_read(sent)` idiom works.
The emulator's architecture doc currently plans to call Winsock2 directly
("keep it here until a second project needs it"); iris is that promotion,
done deliberately: the API below is the *contract the emulator already
designed against*, lifted into the library layer so the next tool (client
side of the same instruments, DAQ bridges) reuses it.

## Requirements traced to the consumer

| # | requirement | source (mps-emulator-c) |
|---|-------------|--------------------------|
| R1 | TCP server: bind/listen (default port 23), accept a command client | 00-architecture: Goals, Thread model |
| R2 | Byte-stream recv (line splitting is the caller's job — network thread is a "dumb byte mover") | 00-architecture: Thread model |
| R3 | TCP send of opaque records on the command connection | 00-architecture: Data flow |
| R4 | UDP datagram send to a configured target address | 00-architecture: Goals, Open decision 3 |
| R5 | Sockets non-blocking behind a poll with bounded timeout (~50 ms) so the quit flag is observed promptly | 00-architecture: Lifecycle |
| R6 | Partial-send count reported, so the consumer advances the ring by what was *actually* sent | 01-concurrency: RingBuffer rationale; aether README zero-copy example |
| R7 | No hidden global state: explicit init/shutdown owned by `main` | 00-architecture: Constraints |
| R8 | Client side (`tcp_connect`) and UDP receive — not used by the emulator itself, required by its loopback/end-to-end validation plan and by future client tools | 02-device: Validation 3 |
| R9 | Minimal dependencies; C11; single-header | 00-architecture: Constraints |

## Scope

Plain IPv4 sockets: lifetime, address construction, TCP listen/accept/
connect/send/recv, UDP open/send/recv, close, poll. Thin wrappers over the
OS socket layer — the same category as aether's files/timers/threads.

**Non-goals** (application or platform machinery, not primitives):
- Overlapped I/O / IOCP / io_uring. The consumer is a two-thread design at
  human + 850 Hz rates; readiness polling is sufficient by construction.
- TLS, HTTP, DNS resolution (see decision 4), IPv6 (see decision 3),
  multicast, raw sockets.
- Buffering, line splitting, record framing. These live in the caller
  (aether `str8` + length-prefixed SPSC channels already cover them).
- Multiple-client session management. `tcp_accept` is generic; "one command
  client" is emulator policy, enforced by the emulator.

**Deferred until they have a caller** (aether precedent: `os_get_last_error`
landed in 0.0.12 with no caller and had to be disabled in 0.0.13 — do not
repeat that):
- Detailed error codes / error strings (`net_last_error`). v1 callers react
  identically to every hard error (log, drop the connection).
- `tcp_set_nodelay` opt-out, send/recv buffer sizing knobs, `SO_LINGER`
  control.
- Hostname resolution (`net_resolve`), IPv6.

## Decisions and rationale

1. **Explicit `net_init` / `net_shutdown`, owned by `main`.** Winsock
   requires a per-process `WSAStartup` before any socket call. The tempting
   alternative — lazy init inside the first `tcp_listen` — is hidden global
   state and an un-owned resource, which the consumer's constraints (and
   this codebase's rules) forbid. `net_init` returns `b8`; every socket
   call before it fails cleanly (Winsock returns `WSANOTINITIALISED`; we
   surface it as an ordinary failure). On POSIX both are no-ops that exist
   so callers are portable. Not reference-counted, not thread-safe: call
   once from `main` before threads start, mirror at exit. This is iris's
   only process-global effect, and it is explicit.

2. **`Socket` is a `u64` handle with a +1 bias so `{0}` is invalid.**

   ```c
   typedef struct Socket { u64 handle; } Socket;   /* {0} = invalid */
   ```

   The aether convention is that zero-init is the invalid/empty state
   (`Thread`, `RingBuffer`, views). Raw OS values break that: a POSIX fd of
   `0` is valid (stdin closed and reused), and Winsock's `INVALID_SOCKET`
   is `~0`, not `0`. Storing `os_value + 1` (so `0` means invalid on both
   platforms) preserves the convention for the price of one add per call.
   This deliberately diverges from `Thread`'s `void*` handle: thread
   handles are never `0`/`NULL` on either platform, socket fds are. The
   bias is applied *inside the `os_` layer* by two one-line converters;
   public code and callers only ever see the biased value.

3. **`NetAddr` is IPv4-only, by value, no `sockaddr` in the public API.**

   ```c
   typedef struct NetAddr { u8 ip[4]; u16 port; } NetAddr;
   ```

   `ip` in wire order (`{192,168,1,10}` reads like the address), `port` in
   host order (callers think "port 23", the os layer does `htons`). The
   consumer's instruments live on numeric IPv4 LANs; IPv6 would double the
   os-layer surface for zero current callers. Extension path if it ever
   matters: add a `family` tag + widen `ip[16]` — a source-level change in
   a header-only library, the same deferral already accepted for POSIX
   `Semaphore` storage in the threads design. No `struct sockaddr` leaks
   through the public API, for the same reason no `HANDLE` leaks out of
   aether: platform types stay in the `os_` layer.

4. **Address construction is numeric-only; no DNS in v1.** `net_addr_parse`
   accepts dotted-quad text (`str8_view`, so config strings parse without a
   copy); `net_addr` / `net_addr_any` / `net_addr_loopback` cover literals.
   `getaddrinfo` is a blocking call into a resolver — hidden latency of
   unbounded duration inside what is otherwise a non-blocking API — and the
   consumer configures targets by IP. When a caller genuinely needs names,
   add an explicitly-blocking `net_resolve` rather than smuggling blocking
   behavior into address construction.

5. **Every socket is non-blocking, always; there is no blocking mode.**
   The consumer's lifecycle requires it (R5: never block unboundedly, poll
   with a bounded timeout). Offering both modes doubles the semantics of
   every recv/send and invites the accidental-hang class of bug. One
   exception, contained: `tcp_connect` performs the handshake blocking and
   flips the socket non-blocking before returning (decision 9). Sockets
   returned by `tcp_accept` are put non-blocking explicitly — inheritance
   of `FIONBIO` across `accept` is not guaranteed and must not be relied on.

6. **Result model: a four-state `NetResult` plus an out-count.**

   ```c
   typedef u8 NetResult;
   enum NetResult_
   {
       NetResult_Ok = 0,
       NetResult_WouldBlock,   /* no data / no buffer space right now; poll and retry */
       NetResult_Closed,       /* orderly peer shutdown (TCP recv only)               */
       NetResult_Error,        /* everything else; treat the socket as dead           */
   };
   ```

   BSD's convention overloads return values (`0` = closed for recv but
   valid for send; `-1` + `errno`/`WSAGetLastError` for both would-block
   and hard errors). The three outcomes a caller handles differently —
   retry later, peer went away, socket is dead — become explicit states;
   byte counts travel through an out-parameter and are valid only on
   `NetResult_Ok`. `b8` remains the return type for calls with only two
   outcomes (`net_init`, `net_addr_parse`), matching aether.

7. **Partial sends are `NetResult_Ok` with `*out_sent < data.size`.** Not
   an error, not retried internally. R6 is the reason: the emulator's
   zero-copy path must advance its ring by exactly what left the socket
   (`ring_buffer_advance_read(sent)`). An API that loops internally until
   everything is sent would block unboundedly (violates R5) and destroy
   the caller's accounting. `NetResult_WouldBlock` with `*out_sent == 0`
   means "no buffer space at all this tick".

8. **`TCP_NODELAY` is set on every TCP socket, no opt-out in v1.** The
   command channel is a telnet-style request/reply protocol: Nagle's
   algorithm delays exactly this traffic (small writes, latency-bound).
   Callers that want batching batch at the record layer — the consumer's
   length-prefixed framing already writes whole records with one `send`.
   An opt-out is deferred with the other knobs.

9. **`tcp_connect` blocks; everything else never does.** A non-blocking
   connect completes via poll-for-writability, and Windows' `WSAPoll` has
   a documented defect (fixed only in Windows 10 2004) where a *failed*
   non-blocking connect reports no events at all — the one idiom WSAPoll
   gets wrong. Since `tcp_connect` exists for R8 (tests, client tools)
   where a blocking handshake is acceptable, the simple contract dodges
   the bug entirely: connect blocking (OS-default timeout), then flip
   non-blocking. If an async connect ever becomes a real requirement, it
   gets its own explicit `tcp_connect_begin/…_poll` pair rather than a
   mode flag.

10. **`net_poll` wraps `WSAPoll`/`poll(2)` over small fixed arrays.**

    ```c
    typedef u8 NetPollFlags;
    enum NetPollFlags_
    {
        NetPoll_Read  = BIT(0),   /* readable; on a listener: connection pending */
        NetPoll_Write = BIT(1),   /* writable                                    */
        NetPoll_Error = BIT(2),   /* out only: error/hangup                      */
    };

    typedef struct NetPollItem
    {
        Socket       socket;   /* in                                  */
        NetPollFlags want;     /* in:  events of interest             */
        NetPollFlags got;      /* out: events ready (0 if none)       */
    } NetPollItem;
    ```

    Returns the number of ready items, `0` on timeout, `-1` on error
    (`i32`). The consumer polls two or three sockets; `WSAPoll`'s API shape
    (array of pollfd, timeout in ms) is already the right one, so iris
    keeps it and only translates types. Invalid sockets in the array are
    reported via `NetPoll_Error` in `got` rather than failing the whole
    call, so a "listener + maybe-client" array needs no compaction logic.
    Scalability beyond dozens of sockets (IOCP/epoll territory) is an
    explicit non-goal.

11. **Listener address reuse is per-platform, decided in the os layer.**
    On POSIX, `SO_REUSEADDR` on a listener is near-mandatory (fast restart
    while the old socket sits in `TIME_WAIT`). On Windows, `SO_REUSEADDR`
    means something different and dangerous (silent port hijacking);
    `SO_EXCLUSIVEADDRUSE` is the correct hardening. `os_tcp_listen` does
    the right thing per platform; the public API says nothing about it.
    This is exactly the kind of divergence the `os_` seam exists to hide.

12. **Platform ifdefs live in the `os_` layer only** — same rule as
    threads-design decision 7, adopted wholesale: public IRIS functions are
    platform-free thin wrappers over `internal os_net_*` / `os_tcp_*` /
    `os_udp_*` helpers; inside each helper, one `#if IRIS_OS_WINDOWS`
    branch and a labeled `#error` on the POSIX branch, so the port gets an
    enumerated to-do list at compile time. `internal` (not bare `static`)
    for all helpers; trailing underscore for file-local items that are not
    part of the os surface (`os_socket_from_raw_`, `os_addr_to_sockaddr_`);
    `IRIS_API` on public declarations *and* definitions (required by
    `IRIS_STATIC` / `IRIS_BUILD_DLL` expansion); prototypes use `(void)`.

13. **The ws2_32 binding: runtime `GetProcAddress`, resolved at point of
    use — no import library, no link flags.** Winsock lives in
    `ws2_32.dll`. Options considered:

    | option | pro | con |
    |--------|-----|-----|
    | link the import lib: `#pragma comment(lib, "ws2_32.lib")` + `target_link_libraries(iris INTERFACE ws2_32)` + documented `-lws2_32` | symbol typos fail at link time; zero per-call machinery | the dependency leaks to every consumer through every build system — covering MSVC/clang-cl, CMake consumers, and bare GCC/MinGW takes all three mitigations stacked, and it would be the first link dependency in the aether family |
    | `GetProcAddress` resolved at point of use (aether's `VirtualAlloc2` pattern) | zero consumer link effort on every toolchain, forever; least state (one pinned `HMODULE`); decision 1's pre-init failure contract falls out for free | 2–3 resolve lines per os_ helper; a typo'd export name fails at first use rather than at link (caught by the first test run — the validation plan exercises every helper) |
    | `GetProcAddress` into a function-pointer table cached by `net_init` | hot path pays one indirect call, no lookups; every name verified in one place at `net_init` | ~60–90 lines of centralized loader machinery; every helper needs an explicit loaded-guard to keep decision 1's contract |

    Decision: **resolve at point of use.** An earlier draft of this
    document linked the import library, dismissing runtime loading as
    "the kernelbase trick solved an *availability* problem; ws2_32 is
    universally present — all cost, no benefit." That framing was wrong
    twice over. The benefit was never availability; it is that a
    single-header library's link dependencies are paid by every consumer
    on every toolchain — exactly the cost this codebase's rules say to
    avoid. And the cost — "a hidden function-pointer table" — dissolves
    because iris, alone in the aether family, already has an explicit
    `net_init`/`net_shutdown` lifetime (decision 1): the loading has an
    owner, so nothing is hidden or lazy.

    Mechanism — one deliberate deviation from the `VirtualAlloc2`
    precedent: kernelbase.dll is loaded in every Windows process, so
    aether needs no state at all; ws2_32.dll is *not*, so `net_init` must
    pin it, and the pin needs an owner:

    ```c
    internal HMODULE os_ws2_dll_;   /* pinned by net_init; NULL = net layer down */

    internal FARPROC os_ws2_sym_(const char* name)
    {
        return os_ws2_dll_ ? GetProcAddress(os_ws2_dll_, name) : NULL;
    }

    /* net_init:     LoadLibraryExW(L"ws2_32.dll", NULL,
                         LOAD_LIBRARY_SEARCH_SYSTEM32) + WSAStartup(2.2)
       net_shutdown: WSACleanup + FreeLibrary + NULL the handle
       each helper:  recv_fn precv = (recv_fn)os_ws2_sym_("recv");
                     if (!precv) return NetResult_Error;               */
    ```

    Properties this buys: while `os_ws2_dll_` is NULL (before `net_init`,
    after `net_shutdown`, or if init failed) every helper's resolve
    returns NULL and the call degrades to `{0}` / `NetResult_Error`
    through checks the helpers need anyway — decision 1's "fails cleanly"
    contract costs nothing extra. `ws2_32` is a KnownDLL;
    `LOAD_LIBRARY_SEARCH_SYSTEM32` makes that explicit. Per-call
    resolution cost is sub-microsecond against a 50 ms poll tick; if a
    future caller ever makes it matter, the promotion path is mechanical
    and local to the os layer — cache the hot pairs (`recv`/`send` +
    `WSAGetLastError`) into the table form.

    Enforcement is structural: **nothing in this repo links ws2_32** — no
    pragma, no CMake interface link, not even the tests. Any accidental
    direct Winsock call becomes an unresolved external at link time
    instead of silently reintroducing the dependency.

14. **Header-order hazard, documented not papered over.** `<winsock2.h>`
    conflicts with the legacy `<winsock.h>` that `<windows.h>` pulls in
    *unless* `WIN32_LEAN_AND_MEAN` is defined. aether's implementation
    block already defines `WIN32_LEAN_AND_MEAN`, and iris's implementation
    includes `<winsock2.h>` + `<ws2tcpip.h>` under the same guard — so the
    canonical single-TU pattern (`AETHER_IMPLEMENTATION` +
    `IRIS_IMPLEMENTATION` together) is safe by construction. The failure
    mode that remains: a consumer TU that included a non-lean
    `<windows.h>` *before* the iris implementation gets redefinition
    errors. That is a compile-time failure with a well-known signature,
    called out in the iris.h preamble; no runtime hazard exists.

## Public API (iris.h, NET section after the context block)

```c
/* ------- N E T ------------------------------------------------------------ */

/* Process-wide socket layer lifetime (WSAStartup/WSACleanup on Windows;
   no-ops on POSIX). Call net_init once from main before any other iris
   call and before threads start; mirror with net_shutdown at exit.
   Not reference-counted, not thread-safe. */
IRIS_API b8   net_init(void);
IRIS_API void net_shutdown(void);

typedef struct NetAddr { u8 ip[4]; u16 port; } NetAddr;  /* ip wire-order, port host-order */

IRIS_API NetAddr net_addr(u8 a, u8 b, u8 c, u8 d, u16 port);
IRIS_API NetAddr net_addr_any(u16 port);        /* 0.0.0.0 — all interfaces  */
IRIS_API NetAddr net_addr_loopback(u16 port);   /* 127.0.0.1                 */
IRIS_API b8      net_addr_parse(str8_view dotted_quad, u16 port, NetAddr* out); /* numeric only, no DNS */

typedef struct Socket { u64 handle; } Socket;   /* {0} = invalid */

IRIS_API b8   socket_valid(Socket s);
IRIS_API void socket_close(Socket* s);          /* idempotent; zeroes the handle */

typedef u8 NetResult;
enum NetResult_
{
    NetResult_Ok = 0,
    NetResult_WouldBlock,   /* retry after net_poll; *out counts are 0        */
    NetResult_Closed,       /* orderly peer shutdown (tcp_recv only)          */
    NetResult_Error,        /* socket is dead; close it                       */
};

/* TCP. All sockets are non-blocking; TCP_NODELAY is always set.
   tcp_connect is the one blocking call (handshake only), see design doc. */
IRIS_API Socket    tcp_listen(NetAddr addr, u32 backlog);          /* {0} on failure    */
IRIS_API Socket    tcp_accept(Socket listener, NetAddr* out_peer); /* {0} if none pending (or error) */
IRIS_API Socket    tcp_connect(NetAddr addr);                      /* {0} on failure    */
IRIS_API NetResult tcp_send(Socket s, bytes_view data, u64* out_sent); /* partial sends are Ok */
IRIS_API NetResult tcp_recv(Socket s, void* buf, u64 cap, u64* out_recv);

/* UDP. Datagrams send whole or not at all; oversized datagrams are
   NetResult_Error. udp_open with port 0 = ephemeral send-only socket. */
IRIS_API Socket    udp_open(NetAddr bind_addr);                    /* {0} on failure    */
IRIS_API NetResult udp_send_to(Socket s, NetAddr to, bytes_view datagram);
IRIS_API NetResult udp_recv_from(Socket s, void* buf, u64 cap, u64* out_recv, NetAddr* out_from);

/* Readiness. Returns ready count, 0 on timeout, -1 on error.
   An invalid Socket in items[] reports NetPoll_Error in .got. */
typedef u8 NetPollFlags;
enum NetPollFlags_
{
    NetPoll_Read  = BIT(0),
    NetPoll_Write = BIT(1),
    NetPoll_Error = BIT(2),   /* out only */
};

typedef struct NetPollItem
{
    Socket       socket;
    NetPollFlags want;
    NetPollFlags got;
} NetPollItem;

IRIS_API i32 net_poll(NetPollItem* items, u32 count, u32 timeout_ms);
```

Notes:
- `bytes_view` / `str8_view` / `b8` / `BIT` come from aether.h, which
  iris.h already includes — iris adds no basic types of its own.
- Names are unprefixed (aether style: `thread_create`, not
  `aether_thread_create`): `net_` for module-wide, `socket_` for
  socket-generic, `tcp_`/`udp_` for protocol-specific. None collide with
  libc/Winsock symbols (`Socket` vs `SOCKET`, `net_poll` vs `poll`).

## Internal `os_` surface (implementation, platform block)

All platform ifdefs live behind these; they speak biased `u64` handles
(`0` = failure) and `NetAddr` — the raw `SOCKET`/fd and `sockaddr_in`
conversions happen in file-local `_`-suffixed helpers inside the layer.
Noun-verb naming, as settled in threads-design decision 7.

```c
internal b8        os_net_init(void);
internal void      os_net_shutdown(void);

internal u64       os_tcp_listen(NetAddr addr, u32 backlog);
internal u64       os_tcp_accept(u64 h, NetAddr* out_peer);
internal u64       os_tcp_connect(NetAddr addr);
internal NetResult os_tcp_send(u64 h, const u8* data, u64 len, u64* out_sent);
internal NetResult os_tcp_recv(u64 h, u8* buf, u64 cap, u64* out_recv);

internal u64       os_udp_open(NetAddr bind_addr);
internal NetResult os_udp_send_to(u64 h, NetAddr to, const u8* data, u64 len);
internal NetResult os_udp_recv_from(u64 h, u8* buf, u64 cap, u64* out_recv, NetAddr* out_from);

internal void      os_socket_close(u64 h);
internal i32       os_net_poll(NetPollItem* items, u32 count, u32 timeout_ms);
```

Windows mapping (the POSIX column is the port's to-do list, each branch a
labeled `#error` until then):

| os_ helper | Winsock | POSIX (later) |
|------------|---------|----------------|
| `os_net_init` / `os_net_shutdown` | `WSAStartup(2.2)` / `WSACleanup` | no-op / no-op |
| `os_tcp_listen` | `socket` + `SO_EXCLUSIVEADDRUSE` + `bind` + `listen` + `FIONBIO` | `socket` + `SO_REUSEADDR` + `bind` + `listen` + `O_NONBLOCK` |
| `os_tcp_accept` | `accept` + `FIONBIO` + `TCP_NODELAY` | `accept` + `O_NONBLOCK` + `TCP_NODELAY` |
| `os_tcp_connect` | `socket` + `connect` (blocking) + `FIONBIO` + `TCP_NODELAY` | same shape |
| `os_tcp_send` / `os_tcp_recv` | `send` / `recv`, map `WSAEWOULDBLOCK` | `send(MSG_NOSIGNAL)` / `recv`, map `EAGAIN` |
| `os_udp_*` | `sendto` / `recvfrom` | same, `MSG_NOSIGNAL` on send |
| `os_net_poll` | `WSAPoll` | `poll` |
| `os_socket_close` | `closesocket` | `close` |

POSIX port notes, recorded now so they are not rediscovered: `SIGPIPE` is
handled per-call with `MSG_NOSIGNAL` (a process-wide `signal(SIGPIPE,
SIG_IGN)` would be hidden global state); binding ports < 1024 (the MPS
default of 23!) needs root/capabilities on POSIX, so the emulator must keep
its port configurable; `EINTR` retry loops belong inside the os helpers.

## Usage sketch — the consumer's network thread

The emulator loop, verbatim in iris calls (framing/lines elided; those are
`str8` + ring-channel code on the caller's side):

```c
Socket listener = tcp_listen(net_addr_any(cfg->port), 1);
Socket client   = {0};

while (!atomic_load_acq_u64(&shared->quit))
{
    NetPollItem items[2] = {0};
    u32 n = 0;
    items[n].socket = listener; items[n].want = NetPoll_Read; n++;
    if (socket_valid(client)) { items[n].socket = client; items[n].want = NetPoll_Read; n++; }

    net_poll(items, n, 50);                     /* bounded: quit observed <= 50 ms (R5) */

    if (!socket_valid(client))
        client = tcp_accept(listener, NULL);    /* {0} if nobody knocked this tick */

    if (socket_valid(client))
    {
        u8 buf[512];
        u64 got = 0;
        NetResult r = tcp_recv(client, buf, sizeof(buf), &got);
        if (r == NetResult_Ok && got) { /* append to line splitter -> cmd channel */ }
        if (r == NetResult_Closed || r == NetResult_Error) socket_close(&client);
    }

    /* data channel -> wire, zero copy, honest partial sends (R6) */
    bytes_view rec = /* peek one framed record's payload */;
    if (rec.size && socket_valid(client))
    {
        u64 sent = 0;
        if (tcp_send(client, rec, &sent) == NetResult_Ok)
            ring_buffer_advance_read(data_rb, sent);   /* advance by SENT, not by peeked */
        /* WouldBlock: leave the record in the ring, retry next tick */
    }
}

socket_close(&client);
socket_close(&listener);
```

Every iris call in that loop returns immediately; the only place the thread
rests is the bounded `net_poll`. That property — no unbounded blocking
anywhere on the data path — is the whole reason for decisions 5, 7, and 9.

## Pitfalls to preserve when implementing

- **Advance-by-sent, never advance-by-peeked** (usage sketch above). The
  API supports it via `out_sent`; the emulator's correctness depends on it.
- **`tcp_recv` returning `NetResult_Ok` with `*out_recv == 0` must not
  happen.** Zero bytes from a non-blocking `recv` is either would-block
  (`NetResult_WouldBlock`) or orderly shutdown (`NetResult_Closed`); the
  os layer must map both explicitly, or callers will spin or misdetect
  disconnects.
- **Set `FIONBIO` on accepted sockets explicitly** — non-blocking status
  is not reliably inherited through `accept`.
- **The bias lives in exactly one place.** All handle<->raw conversion in
  the two `_`-suffixed os-layer converters; if `+1` appears anywhere else,
  it is a bug waiting for fd 0.
- **`WSAPoll` cannot be used to complete a non-blocking connect** on
  Windows builds before 10-2004 (silent no-event on failure). The blocking
  `tcp_connect` contract exists to keep this defect unreachable; do not
  "optimize" connect to non-blocking without adding the `SO_ERROR`
  fallback and a test on an affected OS.
- **`udp_send_to` of a datagram larger than the socket buffer / MTU
  policy** returns `NetResult_Error` (`WSAEMSGSIZE`), not a partial count —
  datagrams are all-or-nothing; only TCP has partial sends.
- **Winsock's `recv`/`send` take `int` lengths.** The os layer must clamp
  `u64` sizes to `INT_MAX` per call (precedent: the `os_file_read`
  `MAXWORD`/`MAXDWORD` defect fixed in 0.0.4 — same trap, same fix).

## Validation plan

All loopback, no external network, in `tests/test_networking.c` (harness
already in place; cases slot into `g_cases`). The client side of each test
uses `tcp_connect`/`udp_open` (R8 pays for itself immediately).

1. **Lifecycle:** calls before `net_init` fail cleanly; init/shutdown
   round-trips; `socket_close` on `{0}` and double-close are no-ops.
2. **Address unit tests:** `net_addr_parse` accepts `"192.168.1.10"`,
   rejects garbage, empty views, out-of-range octets; pure function, no
   sockets needed.
3. **TCP round trip:** listen (ephemeral port) → connect → poll listener
   readable → accept → echo a few KB each way with sequence/checksum
   verification (xorshift pattern, same as `spsc_stress`).
4. **Would-block semantics:** recv on an idle connection returns
   `WouldBlock` (not `Closed`, not `Ok`+0); `tcp_accept` with nobody
   pending returns `{0}`.
5. **Orderly close:** peer closes → exactly one `NetResult_Closed`;
   subsequent recv is not `WouldBlock`.
6. **Backpressure / partial sends:** blast data at a non-consuming peer
   until `WouldBlock`; drain; verify totals match and no bytes were
   duplicated or lost — this is the R6 contract under stress.
7. **UDP round trip:** bind ephemeral, `udp_send_to` loopback,
   `udp_recv_from` sees the payload and the sender's address; oversized
   datagram → `NetResult_Error`.
8. **Poll timeout:** empty poll for 50 ms returns 0 and
   `time_mark`/`time_elapsed_sec` confirms ≥ ~45 ms (lower bound only;
   CI jitter).
9. **Two-thread integration:** producer thread pushes framed records into
   a `RingBuffer`; network "thread" (the test main) does the usage-sketch
   loop against a consumer thread holding the client socket — the emulator
   architecture in miniature, exercising threads + atomics + ring + iris
   together.

## Follow-ups after landing

- CMake: `target_link_libraries(iris INTERFACE ws2_32)` under `if(WIN32)`
  (decision 13).
- iris.h preamble: document `-lws2_32` for non-CMake GCC builds and the
  `<windows.h>`-before-implementation ordering hazard (decision 14).
- README (this repo): an IRIS section once the API lands — mirror the
  Atomics/Ring/Threads pattern, with the zero-copy `peek -> send ->
  advance(sent)` example shown against a real socket.
- mps-emulator-c: revisit its Open Decision 2 (socket layer location) —
  the answer becomes "use iris", and its `src/net.c` shrinks to session
  policy (one client, line splitting, channel plumbing).
- Later, with callers: `net_last_error`, `net_resolve`, IPv6, socket
  option knobs; POSIX branch of every os helper (the table above is the
  to-do list), then the whole suite under WSL + TSan alongside the
  threads port.
