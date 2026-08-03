# Design: Console Signal Handler

Status: **designed, not yet implemented** (2026-08-03).
Prerequisites already landed: the `AETHER_API` linkage modes (extern / static /
dllexport / dllimport), `internal`/`AETHER_OS_WINDOWS` platform macros, `FATAL`.
No dependency on `RingBuffer`, atomics, or `Thread` despite sitting in the same
file and the same neighborhood as THREADS.

## Motivation

Driving caller: `mps-emulator-c`, a downstream consumer of `aether`, needs
Ctrl+C to trigger a graceful shutdown -- set a quit flag, unblock a thread
parked in a blocking `iris` socket call via `socket_close`, join everything,
release resources. Every other OS-specific thing that application touches is
already hidden behind `aether`/`iris` (`Thread` wraps `CreateThread`, `Socket`
wraps Winsock, `HighResTimer` wraps `QueryPerformanceCounter`) -- none of it
leaks a Win32 type into application code. `SetConsoleCtrlHandler` would be the
first crack in that separation if the application called it directly. This
document specs the `aether`-side wrapper that keeps it that way.

## Scope

One function pointer type, one install call, one uninstall call. A process-wide
singleton -- there is exactly one active handler at a time, matching how
`SetConsoleCtrlHandler` is actually used in practice here (not how it's
capable of being used -- see Non-goals).

**Non-goals:**
- Handler chaining. Win32 supports registering multiple handlers via repeated
  `SetConsoleCtrlHandler(fn, TRUE)` calls, each tried in turn. `aether` does
  not expose this -- `console_signal_install` fails if a handler is already
  installed. One caller, one handler, matches the one known use case.
- Per-event-type dispatch. `CTRL_C_EVENT`, `CTRL_BREAK_EVENT`,
  `CTRL_CLOSE_EVENT`, `CTRL_LOGOFF_EVENT`, and `CTRL_SHUTDOWN_EVENT` all
  invoke the same `fn(user)` -- no `DWORD ctrl_type` reaches the caller.
  Exposing it would leak a Win32 type into the public API, which defeats the
  purpose of this file existing.
- POSIX signal handling. `aether` is Windows-only today
  (`#if !AETHER_OS_WINDOWS #error`, already in the header). See Follow-ups for
  how this maps to `sigaction` later.

**Deferred until they have a caller:**
- Passing the raw event type through (would need a public enum mirroring
  `CTRL_C_EVENT` etc.).
- `console_signal_is_installed()` query. Trivial to add; no current caller
  needs it.

## Decisions and rationale

1. **Single generic notification, no event-type discrimination.** The only
   known caller treats every one of the five Win32 console events identically
   ("something external wants this process to stop"). Distinguishing them
   would mean designing a public enum for a caller that doesn't exist yet --
   exactly the kind of speculative API surface to avoid.

2. **File-scope singleton storage, not a per-call handshake like
   `thread_create`'s `ThreadStart_`.** Same underlying problem as
   `threads-design.md` decision 1 -- the OS callback signature
   (`BOOL WINAPI (*)(DWORD)`) has no slot for a user-context pointer, so it
   has to be smuggled in some other way. `thread_create` solves this with a
   stack-allocated handshake because each call creates an independent thread
   with its own `fn`/`user` pair (a genuinely multi-instance problem).
   `SetConsoleCtrlHandler` is not multi-instance the way this library uses it
   (see Non-goals: no chaining) -- there is only ever one active registration,
   so one set of file-scope statics is the right-sized equivalent, not an
   under-engineered one.

3. **Storage is written by the public function, read by the platform-specific
   thunk.** This is a deliberate, narrow exception to
   `threads-design.md` decision 7 ("platform ifdefs live in the `os_` layer
   only; public functions are platform-free"). The exception is unavoidable
   here: the callback's parameter list has nowhere to carry the user pointer,
   so something outside it must hold `fn`/`user`, and there is no reason to
   duplicate that storage inside the `os_` layer when the public layer already
   owns writing it exactly once per install.

4. **`fn`/`user` are published through a dedicated atomic gate, not left as
   plain writes.** An earlier draft of this decision argued that the kernel
   transition inside `SetConsoleCtrlHandler` was itself a sufficient memory
   barrier, so `console_signal_fn_`/`console_signal_user_` could stay plain
   globals. That conflates two different things: the calling thread's own
   instruction ordering (which the syscall genuinely does fix) with
   cross-thread *visibility* to the OS-created thread that later runs the
   thunk -- a guarantee no C11/C++11 abstract machine recognizes, since
   nothing about `SetConsoleCtrlHandler` is an `atomic_*`/`volatile`/mutex
   operation the standard knows about. By the letter of the standard, an
   unsynchronized plain write on one thread and a plain read on another is a
   data race, even though it would almost certainly behave on real x86-64
   Windows. `console_signal_fn_`/`console_signal_user_` stay plain values
   (no pointer round-tripping through `u64` -- a function pointer's
   convertibility to an integer type is its own can of worms), gated by one
   dedicated atomic word instead:

   ```c
   global console_signal_fn console_signal_fn_    = NULL;
   global void*             console_signal_user_  = NULL;
   global u64               console_signal_ready_ = 0;      /* atomic gate */
   ```

   `console_signal_install` writes `fn_`/`user_` first, then
   `atomic_store_rel_u64(&console_signal_ready_, 1)`; the thunk does
   `atomic_load_acq_u64(&console_signal_ready_)` before trusting `fn_`. This
   is the exact publish-once pattern `thread_create`'s stack handshake and
   the README's Worker/stop-flag example already use elsewhere in `aether`
   -- one release store after the plain writes, one acquire load before the
   plain reads, and the pairing is what makes everything written before the
   release store visible to whatever observes the acquire load, per the
   C11/C++11 memory model this codebase already holds itself to (the
   pedantic-conformance discipline behind `verify_c11`/`verify_cxx11`).
   This reasoning is still specific to "written once, before the gate
   opens" -- it does **not** generalize to concurrent install/uninstall, or
   to uninstalling while a callback might be in flight, which remains
   unsupported (see Pitfalls); the gate fixes the first-install visibility
   question, not concurrent mutation.

5. **The thunk always returns `TRUE` once a handler is installed.** For
   `CTRL_C_EVENT`/`CTRL_BREAK_EVENT`, `TRUE` suppresses Windows' default
   action (immediate termination) -- the entire point, since it buys the
   caller time to run its own shutdown sequence. For
   `CTRL_CLOSE_EVENT`/`CTRL_LOGOFF_EVENT`/`CTRL_SHUTDOWN_EVENT`, `TRUE` does
   **not** prevent eventual termination -- Windows enforces its own grace
   period regardless (see Pitfalls) -- but it is still the correct return
   value: "handled, don't fall through to the next handler or default
   action."

6. **Named `install`/`uninstall`, not `_create`/`_release`.** The rest of
   `aether`'s create/release pairing names an owned resource -- a handle, an
   allocation. This wraps a global OS hook with no handle of its own;
   "install a handler" describes what's actually happening better than
   "create" one.

## Public API (header, new CONSOLE SIGNAL section after THREADS)

```c
/* ------- C O N S O L E   S I G N A L ---------------------------------------- */

typedef void (*console_signal_fn)(void* user);

/* Installs a process-wide console control handler covering Ctrl+C, Ctrl+Break,
   console-close, logoff, and shutdown -- aether does not distinguish between
   them (see design doc, decision 1). fn runs on an OS-created thread that is
   neither the calling thread nor any aether Thread; keep it tiny (flag-set
   only) -- see design doc, Pitfalls. Only one handler may be installed at a
   time. Returns false if one already is, or if the OS call fails. */
AETHER_API b8 console_signal_install(console_signal_fn fn, void* user);

/* Removes the installed handler. Safe to call when none is installed. */
AETHER_API void console_signal_uninstall(void);
```

## Internal `os_` surface (platform layer, near the other `os_` helpers)

```c
internal b8   os_console_signal_install(void);   /* registers the thunk; false on OS failure */
internal void os_console_signal_uninstall(void);
```

`console_signal_fn_`/`console_signal_user_`/`console_signal_installed_`/
`console_signal_ready_` are file-scope statics (decision 3), declared
alongside these `os_` functions but **not** themselves inside an
`#if AETHER_OS_WINDOWS` block -- only the thunk and the two
`SetConsoleCtrlHandler` calls are platform-specific. `console_signal_ready_`
is the one atomic word among them (decision 4); the rest stay plain,
gated by it.

## Reference implementation sketch

### Platform section (near the other `os_` helpers, e.g. after `os_thread_*`)

```c
global console_signal_fn console_signal_fn_        = NULL;
global void*             console_signal_user_      = NULL;
global u64               console_signal_ready_     = 0;      /* atomic gate, decision 4 */
global b8                console_signal_installed_ = false;

#if AETHER_OS_WINDOWS
internal BOOL WINAPI os_console_ctrl_thunk_(DWORD ctrl_type)
{
    (void)ctrl_type; /* intentionally unused -- decision 1, not an oversight */
    if (atomic_load_acq_u64(&console_signal_ready_) && console_signal_fn_)
        console_signal_fn_(console_signal_user_);
    return TRUE; /* see decision 5 */
}
#endif // AETHER_OS_WINDOWS

internal b8 os_console_signal_install(void)
{
#if AETHER_OS_WINDOWS
    return SetConsoleCtrlHandler(os_console_ctrl_thunk_, TRUE) != 0;
#else
    #error "AETHER: OS console signal install not implemented for this platform"
#endif
}

internal void os_console_signal_uninstall(void)
{
#if AETHER_OS_WINDOWS
    SetConsoleCtrlHandler(os_console_ctrl_thunk_, FALSE);
#else
    #error "AETHER: OS console signal uninstall not implemented for this platform"
#endif
}
```

### Public wrappers (new CONSOLE SIGNAL block, after THREADS) -- no platform ifdefs

```c
/* ------------------------------------------------------------------------- */
/* --- C O N S O L E   S I G N A L ------------------------------------------ */
/* ------------------------------------------------------------------------- */

AETHER_API b8 console_signal_install(console_signal_fn fn, void* user)
{
    if (!fn) return false;
    if (console_signal_installed_) return false; /* single active handler only -- see Non-goals */

    console_signal_fn_   = fn;
    console_signal_user_ = user;
    atomic_store_rel_u64(&console_signal_ready_, 1); /* publish before the OS can observe it, decision 4 */

    if (!os_console_signal_install())
    {
        atomic_store_rel_u64(&console_signal_ready_, 0);
        console_signal_fn_   = NULL;
        console_signal_user_ = NULL;
        return false;
    }

    console_signal_installed_ = true;
    return true;
}

AETHER_API void console_signal_uninstall(void)
{
    if (!console_signal_installed_) return;
    os_console_signal_uninstall();
    atomic_store_rel_u64(&console_signal_ready_, 0); /* close the gate before clearing */
    console_signal_installed_ = false;
    console_signal_fn_        = NULL;
    console_signal_user_      = NULL;
}
```

`windows.h` should already be included in the platform block from the
THREADS/TIMING implementation; if it isn't yet, add it once at the top of the
`#if AETHER_OS_WINDOWS` platform section, not locally here.

## Pitfalls to preserve when implementing

- **`fn` runs on an OS-created thread** -- not the thread that called
  `console_signal_install`, not any `Thread` the application created. It can
  fire concurrently with literally anything else in the process. Keep it to
  the bare minimum (a flag set, e.g. `atomic_store_rel_u64(&sync->quit, 1)`
  in the motivating caller) -- no `printf`, no allocation, nothing that could
  block or that assumes exclusive access to program state. Same discipline as
  a POSIX signal handler, even though Win32 doesn't mechanically enforce
  async-signal-safety the way POSIX does.
- **`CTRL_CLOSE_EVENT`/`CTRL_LOGOFF_EVENT`/`CTRL_SHUTDOWN_EVENT` carry a real
  termination grace period regardless of the thunk's return value** --
  historically on the order of a few seconds. If the whole process hasn't
  exited by then, Windows kills it anyway. This is out of `aether`'s hands --
  it's the caller's responsibility to make sure whatever `fn` triggers (a
  quit-flag wait loop, thread joins, resource release) actually finishes
  quickly -- but the budget is worth documenting on the public declaration so
  callers know it exists.
- **The atomic gate only covers the *first* install's publish, not
  concurrent mutation.** Decision 4's `console_signal_ready_` gate makes a
  successful `console_signal_install` call's `fn_`/`user_` writes visible to
  the thunk soundly. It does not extend to calling `install`/`uninstall`
  concurrently with each other, or uninstalling while a callback might be in
  flight. Don't try to make that safe -- document it as unsupported (install
  once, during single-threaded startup, before other threads that might race
  with it are spawned) and leave it there.
- **Don't silence the unused-parameter warning on `ctrl_type` some other
  way** -- the `(void)ctrl_type` cast is a marker that decision 1 (no
  per-event dispatch) was a choice, not an oversight. If a future change adds
  event-type dispatch, that line is exactly where to start.

## Validation plan

- **Automated:** install a test handler that increments a counter (or signals
  a `Semaphore`); call `GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0)` from the
  same process; assert the counter changed within a short timeout.
  **Caveat, flag plainly:** `GenerateConsoleCtrlEvent` signals the entire
  console process group, which includes the parent shell running the test
  suite unless the test binary was launched with `CREATE_NEW_PROCESS_GROUP`.
  Expect this to be the flaky/CI-unfriendly case in the suite -- same caveat
  class as the semaphore timeout test in `threads-design.md`.
- **Uninstall test:** uninstall, then repeat the `GenerateConsoleCtrlEvent`
  check, assert the counter does *not* change.
- **Double-install test:** install twice without an intervening uninstall;
  assert the second call returns `false` and the first handler is still the
  one that fires.
- **Manual:** a small example (mirroring `examples/example.c` /
  `examples/timer.c`) that installs a handler, loops printing "waiting..." on
  a sleep-and-check of the flag, and exits cleanly on signal; developer
  confirms by pressing Ctrl+C interactively.

## Follow-ups after landing

- README: new section, same pattern as the existing Atomics/Ring/Threads
  sections.
- CHANGELOG: under Added.
- `mps-emulator-c` is the motivating caller -- once this lands and is tagged,
  its `main.c` can drop the file-scope `Sync*` global it would otherwise need
  for the exact same "OS callback has no user param" reason, and never needs
  to include `windows.h` directly at all.
- POSIX port (`posix-port.md`): **not** a drop-in `sigaction(SIGINT, ...)` /
  `sigaction(SIGTERM, ...)` swap inside `os_console_signal_install`/
  `os_console_signal_uninstall`. A raw signal handler runs synchronously on
  whichever thread was interrupted, at an arbitrary point in its execution,
  under full async-signal-safety restrictions -- a materially different (and
  more dangerous) execution context than the one the public API promises
  ("`fn` runs on an OS-created thread that is neither the calling thread nor
  any aether `Thread`"). Upholding that guarantee on POSIX needs a dedicated
  signal-handling thread instead: block `SIGINT`/`SIGTERM` on every other
  thread at startup, park one thread in `sigwait`/`sigtimedwait`, and call
  `fn` from there. That thread is `os_console_signal_install`'s POSIX
  implementation detail, still behind the same platform-neutral public API
  and `console_signal_fn` typedef -- just more machinery than the Windows
  side needed. Noting this now so the POSIX port doesn't quietly ship a
  weaker guarantee than the header documents.
