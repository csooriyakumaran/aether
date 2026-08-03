# Design: IRIS Minimal Networking (sockets)

Status: DRAFT (2026-08-02).
Consumer driving the requirements: `mps-emulator-c` (`docs/design/00-architecture.md`,
`01-concurrency.md`, `02-device.md` in that repo). Everything that repo's
network thread needs, and nothing it does not.
Supersedes `iris-networking-design-single-threaded.md` — that doc's
non-blocking + `net_poll` mechanism (its decisions 5, 9, 10) existed to let
one thread multiplex several sockets under a bounded wait. `aether` has
since grown a real `Thread` API (`thread_create`/`thread_join`, atomics),
so the network thread can now be a dedicated OS thread that a coordinator
cancels directly, instead of a single thread polling a shared quit flag
every tick. Decisions that did not depend on that mechanism (address
model, result-model shape for partial sends, `TCP_NODELAY`, ws2_32
loading, the os-layer seam) carry over unchanged and are only summarized
here — see the superseded doc for their full rationale.

## Motivation

The MPS4200 emulator runs one **network thread** that owns every socket and
moves bytes between the wire and three SPSC ring channels (cmd / rsp /
data). The main thread owns lifecycle:

```
   main thread                          network thread (aether Thread)
   ────────────                         ──────────────────────────────
   thread_create(net_fn, &shared)  ───▶  loop: tcp_accept (blocks)
                                          loop: tcp_recv / tcp_send (block)
   on shutdown:
     socket_close(&shared.listener) ──▶  blocked accept/recv/send return
     socket_close(&shared.client)        NetResult_Error / Closed; thread exits
   thread_join(&net_thread)        ◀───  thread returns
```

Its needs are unchanged from the original design: a TCP listener with a
single command client, an optional UDP data target, and honest partial-send
reporting so the zero-copy `ring_buffer_peek -> send ->
ring_buffer_advance_read(sent)` idiom works. What changed is *how* the
thread's lifetime is bounded: not by polling, but by the coordinator
interrupting blocked calls and joining the thread.

## Requirements traced to the consumer

Same R1–R9 as the superseded doc, with R5's mechanism updated:

| # | requirement | source (mps-emulator-c) |
|---|-------------|--------------------------|
| R1 | TCP server: bind/listen (default port 23), accept a command client | 00-architecture: Goals, Thread model |
| R2 | Byte-stream recv (line splitting is the caller's job) | 00-architecture: Thread model |
| R3 | TCP send of opaque records on the command connection | 00-architecture: Data flow |
| R4 | UDP datagram send to a configured target address | 00-architecture: Goals, Open decision 3 |
| R5 | Network thread exits promptly on shutdown — no call on the data path can block past the point where the coordinator wants it to stop | 00-architecture: Lifecycle |
| R6 | Partial-send count reported, so the consumer advances the ring by what was *actually* sent | 01-concurrency: RingBuffer rationale |
| R7 | No hidden global state: explicit init/shutdown owned by `main` | 00-architecture: Constraints |
| R8 | Client side (`tcp_connect`) and UDP receive, for validation/tooling | 02-device: Validation 3 |
| R9 | Minimal dependencies; C11; single-header | 00-architecture: Constraints |
| R10 | *(new)* Network thread lifecycle uses `aether`'s `Thread` API; shutdown is cancellation from the coordinator, not a self-polled flag | this doc |

## Scope

Unchanged: plain IPv4 sockets, TCP listen/accept/connect/send/recv, UDP
open/send/recv, close, thin wrappers over the OS socket layer.

**Non-goals**, in addition to the superseded doc's list (TLS/HTTP/DNS/IPv6/
multicast/raw sockets, buffering/framing, IOCP/io_uring):
- **Thread-per-connection / a worker pool.** R1 is one command client,
  singular — "one command client" is emulator policy, and nothing in the
  traced requirements needs concurrent multi-client handling. One network
  thread stays the shape; only its cancellation mechanism changed. Revisit
  if a future caller genuinely needs concurrent connections.

**Deferred until they have a caller** (unchanged from the superseded doc):
detailed error codes/strings, socket option knobs beyond `TCP_NODELAY`,
hostname resolution, IPv6.

## Decisions and rationale

Decisions 1–4 carry over unchanged from `iris-networking-design-single-threaded.md`
(explicit `net_init`/`net_shutdown` owned by `main`; `Socket` as a
+1-biased `u64` handle so `{0}` is invalid; `NetAddr` as IPv4-only,
by-value, wire-order IP / host-order port; numeric-only address
construction with no DNS). Nothing about threading changes their rationale.

5. **Every socket call blocks; the network thread is a dedicated OS
   thread, cancelled by the coordinator.** With `Thread` available, R5
   ("exit promptly on shutdown") no longer requires every call to return
   quickly on its own — it requires the coordinator to be *able* to force
   a blocked call to return. On Windows, `closesocket()` on a socket that
   another thread is blocked on (`accept`, `recv`, or `send`) is documented
   to cancel that pending call and return an error to the blocked thread —
   this is the interrupt primitive that non-blocking + `net_poll` used to
   substitute for, provided by the OS. The shutdown sequence is: coordinator
   calls `socket_close` on the listener and any live client, then
   `thread_join`s the network thread. Offering a non-blocking mode
   alongside this would resurrect the doubled semantics decision 5 of the
   superseded doc rejected non-blocking for in the first place — there is
   still exactly one mode, it just changed which one.

6. **Result model narrows to three states.** `NetResult_WouldBlock` is
   removed — there is no "this call would have blocked" outcome once every
   call blocks by construction.

   ```c
   typedef u8 NetResult;
   enum NetResult_
   {
       NetResult_Ok = 0,
       NetResult_Closed,   /* orderly peer shutdown, or we were cancelled */
       NetResult_Error,    /* socket is dead                              */
   };
   ```

   A coordinator-triggered cancellation and a real peer/socket error both
   surface as `NetResult_Error` (or `Closed`, for a `recv` that raced a
   `socket_close`) — v1 callers already react identically to every hard
   error (log, drop the connection; superseded doc, "deferred" section),
   so no separate `Cancelled` state is added without a caller that would
   act on it differently.

7. **Partial sends remain `NetResult_Ok` with `*out_sent < data.size`.**
   Unchanged in outcome from the superseded doc's decision 7, but the
   reasoning is worth restating under blocking semantics: it is tempting to
   assume a blocking `send` delivers the whole buffer or fails, but that is
   not guaranteed by the socket API, and — more importantly — iris must
   still not loop internally until everything is sent. A stalled peer would
   turn that internal loop into exactly the unbounded block R5 exists to
   prevent, just moved one level down from where polling used to prevent
   it. The caller keeps driving retries via its own record loop, same as
   before.

8. **`TCP_NODELAY` is set on every TCP socket, no opt-out in v1.**
   Unchanged from the superseded doc's decision 8; orthogonal to threading.

9. **`tcp_connect` is no longer a special case.** The superseded doc's
   decision 9 made `tcp_connect` the one blocking call specifically to dodge
   a Windows `WSAPoll` defect in non-blocking connect completion. With
   nothing non-blocking left, there is no separate non-blocking-connect
   code path for that defect to live in — the carve-out and the pitfall it
   documented both disappear together.

10. **`net_poll` / `NetPollItem` / `NetPollFlags` are removed entirely.**
    They existed to let one thread wait on several sockets under one
    bounded timeout. Each socket is now blocked on by exactly one thread,
    so there is nothing left to multiplex. (This also retires the question
    from the prior design review of whether `NetPoll_Write` earns its
    place — moot, the whole poll surface is gone.)

11. **Cancellation is `socket_close()` from the coordinating thread;
    single-owner-closes is a hard invariant.** A worker that receives
    `NetResult_Error`/`Closed` from its own peer/network failure must
    *return*, not call `socket_close` itself — the coordinator is the only
    caller of `socket_close` for handles shared across threads, because it
    is the one using that same call to cancel the worker in the first
    place. Two threads racing to close the same handle is a double-close.
    The handoff of "which socket is currently live" (the `client` handle
    changes on every accept) goes through `aether`'s existing
    `atomic_store_rel_u64` / `atomic_load_acq_u64` on the `Socket.handle`
    field — no new synchronization primitive needed, and no need to wait
    on the semaphore/mutex work still on `aether`'s roadmap.
    **Portability note:** this cancellation contract leans on a Windows
    guarantee (`closesocket()` reliably interrupts pending blocking calls
    issued by other threads). Raw POSIX `close()` does not give the same
    safety — behavior when one thread closes an fd another thread is
    blocked on is unspecified, and fd-number reuse by an unrelated `socket()`
    call racing the same close is a real hazard. The POSIX port must give
    `os_socket_close` (or a POSIX-only cancellation path inside it) the same
    externally-visible contract through a different mechanism — that is an
    `os_`-layer problem, same shape as the `SO_REUSEADDR` /
    `SO_EXCLUSIVEADDRUSE` divergence in decision 12; it does not change the
    public API.

Decisions 12, 13, and 15 carry over unchanged from the superseded doc's
decisions 11, 12, and 14 (listener address reuse decided per-platform in
the `os_` layer; platform ifdefs confined to the `os_` layer with
noun-verb naming; the `WIN32_LEAN_AND_MEAN` / `<winsock2.h>` header-order
hazard). None of them depend on blocking-vs-non-blocking.

14. **`ws2_32` is self-hosted: resolved and loaded at runtime, never
    linked.** `net_init` pins `ws2_32.dll` with `LoadLibraryExW(...,
    LOAD_LIBRARY_SEARCH_SYSTEM32)`; every `os_` helper resolves the exact
    Winsock function it needs via `GetProcAddress` at the point of use,
    mirroring the `VirtualAlloc2` pattern aether already uses for
    kernelbase (decision 13 of the superseded doc). Nothing in this repo's
    build links `ws2_32` — no `-lws2_32`, no `target_link_libraries`. This
    landed directly rather than going through the interim direct-link step
    an earlier draft of this decision described: the mechanism turned out
    to be small enough (a pinned `HMODULE`, ~15 function-pointer typedefs,
    one resolve helper) that deferring it bought nothing. See
    "Implementation: self-hosting ws2_32" below for the mechanism and the
    full symbol list.

## Public API (iris.h, NET section after the context block)

```c
/* ------- N E T ------------------------------------------------------------ */

IRIS_API b8   net_init(void);
IRIS_API void net_shutdown(void);

typedef struct NetAddr { u8 ip[4]; u16 port; } NetAddr;  /* ip wire-order, port host-order */

IRIS_API NetAddr net_addr(u8 a, u8 b, u8 c, u8 d, u16 port);
IRIS_API NetAddr net_addr_any(u16 port);
IRIS_API NetAddr net_addr_loopback(u16 port);
IRIS_API b8      net_addr_parse(str8_view dotted_quad, u16 port, NetAddr* out);

typedef struct Socket { u64 handle; } Socket;   /* {0} = invalid */

IRIS_API b8   socket_valid(Socket s);
IRIS_API void socket_close(Socket* s);   /* idempotent; zeroes the handle. Cross-thread
                                             cancellation for any thread blocked on *s —
                                             see decision 11. Only the coordinator calls
                                             this on handles shared across threads. */

typedef u8 NetResult;
enum NetResult_
{
    NetResult_Ok = 0,
    NetResult_Closed,   /* orderly peer shutdown, or socket_close() cancelled us */
    NetResult_Error,    /* socket is dead; close it                             */
};

/* TCP. All calls block. TCP_NODELAY is always set. tcp_accept returns {0}
   only on cancellation or a hard failure — never "nobody pending yet". */
IRIS_API Socket    tcp_listen(NetAddr addr, u32 backlog);
IRIS_API Socket    tcp_accept(Socket listener, NetAddr* out_peer);
IRIS_API Socket    tcp_connect(NetAddr addr);
IRIS_API NetResult tcp_send(Socket s, bytes_view data, u64* out_sent); /* partial sends are Ok */
IRIS_API NetResult tcp_recv(Socket s, void* buf, u64 cap, u64* out_recv);

/* UDP. Datagrams send whole or not at all; oversized datagrams are
   NetResult_Error. udp_open with port 0 = ephemeral send-only socket. */
IRIS_API Socket    udp_open(NetAddr bind_addr);
IRIS_API NetResult udp_send_to(Socket s, NetAddr to, bytes_view datagram);
IRIS_API NetResult udp_recv_from(Socket s, void* buf, u64 cap, u64* out_recv, NetAddr* out_from);
```

Notes (unchanged from the superseded doc): `bytes_view`/`str8_view`/`b8`
come from `aether.h`; names are unprefixed (`net_`, `socket_`, `tcp_`,
`udp_`), none colliding with libc/Winsock symbols.

## Internal `os_` surface (implementation, platform block)

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
```

`os_net_poll` is gone. `os_tcp_listen` and `os_tcp_accept` no longer set
`FIONBIO` (or `O_NONBLOCK` on the future POSIX branch) at all — blocking is
already the OS default, so that step in each helper simply drops out:

| os_ helper | Winsock | POSIX (later) |
|------------|---------|----------------|
| `os_net_init` / `os_net_shutdown` | `WSAStartup(2.2)` / `WSACleanup` | no-op / no-op |
| `os_tcp_listen` | `socket` + `SO_EXCLUSIVEADDRUSE` + `bind` + `listen` | `socket` + `SO_REUSEADDR` + `bind` + `listen` |
| `os_tcp_accept` | `accept` + `TCP_NODELAY` | `accept` + `TCP_NODELAY` |
| `os_tcp_connect` | `socket` + `connect` (blocking) + `TCP_NODELAY` | same shape |
| `os_tcp_send` / `os_tcp_recv` | `send` / `recv` | `send(MSG_NOSIGNAL)` / `recv` |
| `os_udp_*` | `sendto` / `recvfrom` | same, `MSG_NOSIGNAL` on send |
| `os_socket_close` | `closesocket` (cancels other threads' pending calls) | `close` — needs a cancellation-safe path, see decision 11 |

## Implementation: self-hosting ws2_32

Landed in `iris.h` as described in decision 14 — this section records the
mechanism and the full symbol list, not a future plan.

**Mechanism:** mirror aether's `VirtualAlloc2` pattern (`aether.h`,
ring-buffer memory helpers) with the one addition ws2_32 needs and
kernelbase doesn't — `kernelbase.dll` is a Known DLL already resident in
every process, so aether only ever *looks up* it (`GetModuleHandleW`);
`ws2_32.dll` is not resident by default, so iris must *load* it.
`net_init`/`net_shutdown` (decision 1) are the natural owner of that
load/unload pair, since they already own the process-wide socket-layer
lifetime:

```c
internal HMODULE os_ws2_dll_;   /* pinned by os_net_init; NULL = net layer down */

internal FARPROC os_ws2_sym_(const char* name)
{
    return os_ws2_dll_ ? GetProcAddress(os_ws2_dll_, name) : NULL;
}
/* os_net_init:     LoadLibraryExW(L"ws2_32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32)
                     then resolve+call WSAStartup(2.2); FreeLibrary + return false on failure
   os_net_shutdown: resolve+call WSACleanup, FreeLibrary, NULL the handle
   every other helper: `send_fn psend = (send_fn)os_ws2_sym_("send");
                        if (!psend) return NetResult_Error;` — same
                        typedef/resolve/null-check/invoke shape aether
                        already uses per call site, just resolving against
                        `os_ws2_dll_` instead of `GetModuleHandleW` */
```

One deviation from aether's exact shape is deliberate: aether's kernelbase
case resolves 3 functions, each in one helper; ws2_32 needs ~15 across
several helpers, so the single-line `os_ws2_sym_` module lookup is shared
rather than repeating `os_ws2_dll_ ? GetProcAddress(...) : NULL` inline at
every one of ~15 call sites. This is not the rejected "60–90 line
function-pointer table" (decision 13 of the superseded doc) — it caches
nothing and adds no state beyond the one `HMODULE` `net_init` already has
to own.

**Full function-load list** — every ws2_32 export the current `os_`
surface (table above) actually calls, traced to the helper(s) that need
it:

| # | function | used by | why |
|---|----------|---------|-----|
| 1 | `WSAStartup` | `os_net_init` | per-process Winsock init (decision 1) |
| 2 | `WSACleanup` | `os_net_shutdown` | mirror of 1 |
| 3 | `socket` | `os_tcp_listen`, `os_tcp_connect`, `os_udp_open` | create the OS socket |
| 4 | `closesocket` | `os_socket_close` | release + cross-thread cancellation (decision 11) |
| 5 | `bind` | `os_tcp_listen`, `os_udp_open` | attach a local address |
| 6 | `listen` | `os_tcp_listen` | mark a bound socket as a listener |
| 7 | `accept` | `os_tcp_accept` | pull one connection off the listener |
| 8 | `connect` | `os_tcp_connect` | client-side handshake |
| 9 | `send` | `os_tcp_send` | TCP write, partial-count aware |
| 10 | `recv` | `os_tcp_recv` | TCP read; `0` = orderly close |
| 11 | `sendto` | `os_udp_send_to` | UDP write to an explicit address |
| 12 | `recvfrom` | `os_udp_recv_from` | UDP read + sender address |
| 13 | `setsockopt` | `os_tcp_listen` (`SO_EXCLUSIVEADDRUSE`), `os_tcp_listen`/`os_tcp_accept`/`os_tcp_connect` (`TCP_NODELAY`, decision 8) | |
| 14 | `htons` | any helper building a `sockaddr_in` (`os_tcp_listen`, `os_tcp_connect`, `os_udp_*`) | host→network port order |
| 15 | `ntohs` | `os_tcp_accept`, `os_udp_recv_from` | network→host port order, filling `out_peer`/`out_from` |

`htons`/`ntohs` belong on this list even though they look like intrinsics
— in `<winsock2.h>` they're declared `WINSOCK_API_LINKAGE`, i.e. real
`ws2_32.dll` exports, not compiler builtins. `htonl`/`ntohl` are not
needed: `NetAddr.ip` is already wire-order bytes copied straight into
`sin_addr` (decision 3), never byte-swapped.

**Explicitly excluded**, same "deferred until it has a caller" rule
applied elsewhere in this doc:
- `ioctlsocket` (`FIONBIO`) — was needed only to flip sockets
  non-blocking; gone along with non-blocking mode itself (decision 5).
- `WSAGetLastError` — every hard error already maps uniformly to
  `NetResult_Error`, and no `os_` helper branches on a specific error code
  today (`recv`'s `0` vs. `SOCKET_ERROR` is enough to tell `Closed` from
  `Error`). Add it when `net_last_error` gets a caller.
- `inet_pton` / `getaddrinfo` — `net_addr_parse` is a manual dotted-quad
  parse over `str8_view` (decision 4), not a resolver call.
- `select` / `WSAPoll` — removed with `net_poll` (decision 10).

## Usage sketch — the consumer's network thread

```c
typedef struct NetShared
{
    Socket   listener;   /* published once before the accept loop starts   */
    Socket   client;     /* republished on every accept; {0} when none     */
    RingBuffer* data_rb;
} NetShared;

int network_thread_fn(void* user)
{
    NetShared* shared = user;

    Socket listener = tcp_listen(net_addr_any(shared->cfg_port), 1);
    atomic_store_rel_u64(&shared->listener.handle, listener.handle);

    for (;;)
    {
        NetAddr peer;
        Socket client = tcp_accept(listener, &peer);   /* blocks */
        if (!socket_valid(client)) break;                /* cancelled or hard failure */
        atomic_store_rel_u64(&shared->client.handle, client.handle);

        for (;;)
        {
            u8 buf[512]; u64 got = 0;
            NetResult r = tcp_recv(client, buf, sizeof(buf), &got);
            if (r == NetResult_Ok && got) { /* append to line splitter -> cmd channel */ }
            if (r != NetResult_Ok) break;   /* Closed or Error: fall through, do not self-close */

            bytes_view rec = /* peek one framed record's payload */;
            if (rec.size)
            {
                u64 sent = 0;
                if (tcp_send(client, rec, &sent) == NetResult_Ok)
                    ring_buffer_advance_read(shared->data_rb, sent);
                else break;
            }
        }
        atomic_store_rel_u64(&shared->client.handle, 0);
        /* worker never closes here on the cancellation path — coordinator owns it */
    }
    return 0;
}

/* main */
NetShared shared = { .data_rb = &data_rb };
Thread net_thread = thread_create(network_thread_fn, &shared);

/* ... run ... */

/* shutdown: cancel whatever the network thread is blocked on, then join */
Socket client = { .handle = atomic_load_acq_u64(&shared.client.handle) };
if (socket_valid(client)) socket_close(&client);
Socket listener = { .handle = atomic_load_acq_u64(&shared.listener.handle) };
socket_close(&listener);
thread_join(&net_thread, NULL);
```

Every blocking call the network thread makes can be interrupted from
`main` via `socket_close`; the only place `main` itself blocks is the
final `thread_join`, which is exactly what `join` is for.

## Pitfalls to preserve when implementing

- **Advance-by-sent, never advance-by-peeked** (unchanged from the
  superseded doc). The API supports it via `out_sent`.
- **Single-owner-closes.** A worker thread must never call `socket_close`
  on a handle the coordinator might also close; it observes
  `Closed`/`Error` and returns. Get this wrong and you get a double-close
  race, not a hang — worse, because it is intermittent.
- **`socket_close`'s cross-thread cancellation guarantee is Windows-only
  as documented.** Do not port `os_socket_close` to POSIX by dropping in
  `close()` and assuming the same behavior — verify the cancellation path
  explicitly (test: a thread blocked in `tcp_accept`/`tcp_recv` actually
  unblocks promptly when another thread calls `socket_close` on the same
  handle) before trusting it on that platform.
- **The `+1` bias lives in exactly one place** (unchanged from the
  superseded doc).
- **`udp_send_to` of an oversized datagram** returns `NetResult_Error`,
  not a partial count (unchanged).
- **Winsock's `recv`/`send` take `int` lengths** — clamp `u64` sizes to
  `INT_MAX` in the os layer (unchanged).

## Validation plan

All loopback, no external network, in `tests/test_networking.c`.

1. **Lifecycle:** calls before `net_init` fail cleanly; init/shutdown
   round-trips; `socket_close` on `{0}` and double-close (from the same
   thread) are no-ops.
2. **Address unit tests:** unchanged from the superseded doc.
3. **TCP round trip:** listen (ephemeral port) → connect → accept (now a
   direct blocking call, no poll-then-accept step) → echo a few KB each
   way with sequence/checksum verification.
4. **Cancellation:** spawn a thread blocked in `tcp_accept` on a listener
   with nothing connecting; from the test's main thread call `socket_close`
   on that listener; assert the worker thread's `tcp_accept` returns `{0}`
   promptly (bounded by a generous test timeout, not a poll interval) and
   `thread_join` completes. Repeat for a thread blocked in `tcp_recv` on an
   idle-but-connected socket.
5. **Orderly close:** peer closes → exactly one `NetResult_Closed`;
   subsequent recv is not spuriously `Ok`.
6. **Backpressure / partial sends:** blast data at a non-consuming peer on
   a bounded-size receive buffer (peer thread reads slowly) until sends
   return partial counts; verify totals match and no bytes are duplicated
   or lost — the R6 contract under stress, now without `WouldBlock` as a
   distinct outcome to also verify.
7. **UDP round trip:** unchanged from the superseded doc.
8. **Two-thread integration:** producer thread pushes framed records into
   a `RingBuffer`; a `network_thread_fn`-shaped thread drains it against a
   consumer thread holding the client socket — the emulator architecture
   in miniature, now using real `Thread`s on both sides instead of a
   single polling loop.

## Follow-ups after landing

- ~~CMake: `target_link_libraries(iris INTERFACE ws2_32)` under
  `if(WIN32)`~~ — not needed; `ws2_32` is self-hosted (decision 14), so
  there is no link step to add.
- iris.h preamble: document the `<windows.h>`-before-implementation
  ordering hazard (decision 15, unchanged) — no `-lws2_32` note needed for
  the same reason.
- ~~README: an IRIS section mirroring the Atomics/Ring/Threads pattern~~ —
  done.
- mps-emulator-c: its network thread becomes `thread_create` + this
  design's usage sketch; its own quit-flag polling loop goes away in favor
  of `socket_close`-triggered cancellation at shutdown.
- POSIX port: give `os_socket_close` (or a dedicated cancellation path
  inside it) the same "interrupts other threads' pending calls" contract
  `closesocket()` gives on Windows — decision 11's portability note is the
  starting point — then the rest of the `os_` table above.
