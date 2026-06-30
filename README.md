# AETHER

***the underlying medium from which all else emerges ..***

AETHER is a small single-header core library for personal c/c++ projects. 

It provides a minimal set of common primitives:

- fixed-width type aliases
- assertions and utility macros
- bit flags
- string views and operations (slice, trim, compare, arena-backed copy/format)
- memory arenas (linear allocator with scratch / temporary support)
- file I/O (read a file into an arena)
- memory-mapped files
- ring / circular buffers (virtually-mirrored — reads and writes that wrap the end stay a single contiguous copy)
- timing helpers

The intent is not to be a framework, but rather a lightweight foundation that can be included in other libraries or applications. The motivation is to share primitives between different aerodynamic solver codes.

## Requirements

Single-header with no link-time dependencies. Requires **C11** or **C++11** (newer standards work too).

## Memory Arenas

AETHER provides a simple linear arena allocator. 

An arena reserves a block of virtual address space and commits memory as needed. Allocations are performed by bumping a position pointer forward. Individual allocations are not freed; instead, memory is reclaimed in bulk by popping, clearing, or releasing the arena. 

> [!NOTE]
> Reservation and commit failures (e.g. out of memory) are treated as fatal and abort the program immediately — there is no recoverable error path.

```c
/* reserve 64 MB of virtual address space.
   The simple arena_alloc() API chooses sensible defaults based on build configuration:
   - release: lazy commit (initial commit = 0), no zeroing, no decommit by default
   - debug:  lazy commit, with ArenaFlags_AlwaysZero, ArenaFlags_DebugFillOnClear,
             and ArenaFlags_Decommit enabled */
Arena arena = arena_alloc(MB(64));

/* push an array of 128 u32 values.
   By default pushes follow the arena's zeroing policy:
   - in debug builds (with ArenaFlags_AlwaysZero set) this will be zeroed
   - in release builds the memory is not zeroed unless you opt in via flags or _zero helpers */
u32* values = arena_push_array(&arena, u32, 128);

/* push a single type or struct */
f64* x      = arena_push_t(&arena, f64);

/* Clear the arena, resetting the position pointer to 0
   Memory is still reserved
   Memory is still committed unless ArenaFlags_Decommit is set*/
arena_clear(&arena);

/* release the memory back to the OS */
arena_release(&arena);

```

### Arena Layout

An arena tracks:

| FIELD         | DESCRIPTION                                                         |
| -----         | ------------------------------------------------------------------- |
| base          | Base pointer to the reserved memory region                          |
| reserved_size | Total virtual memory reserved for the arena                         |
| commit_size   | Amount of memory currently commited and accessible                  |
| pos           | Current allocation position in bytes from `base`                    |
| granularity   | Commit page granularity (number of pages per check when configured) |
| flags         | 8-bit flag for controlling arena behaviour                          |

Allocation occur from `base + pos`. The position is advanced after each push.

### Arena Flags

`ArenaFlags` is currently stored as a `u8`. This allows up to 8 independent flags. The type can be widened later if needed. 

| FLAG                          | BIT SET  | DESCRIPTION                                                                 |
| --------------------------    | -------- | --------------------------------------------------------------------------- |
| `ArenaFlags_None`             | `0`      | No special behaviour                                                        |
| `ArenaFlags_Decommit`         | `BIT(0)` | Decommit unused tail memory pages when popping or clearning                 |
| `ArenaFlags_CommitChunked`    | `BIT(1)` | Commit memory in larger chunks (defined by `granularity`)                   |
| `ArenaFlags_AlwaysZero`       | `BIT(2)` | Zero memory for every allocation that follows the arena policy. Default push macros obey this flag; explicit `_zero` helpers force zeroing regardless of policy   |
| `ArenaFlags_DebugFillOnClear` | `BIT(3)` | fill invalidated memory with `0xDD` when popping/clearing                   |


***Example: arena_alloc***

```c
/* reserve 64 MB */
Arena arena = arena_alloc(MB(64));

arena.flags =
    ArenaFlags_Decommit |         /* when popping/clearing, decommit unused pages */
    ArenaFlags_DebugFillOnClear;  /* when popping/clearing, fill any remaining committed memory with 0xDD */
```

***Example: arena_alloc_ex***

For finer control:

```c
u64 initial_commit_size = KB(4);  /* initially commit 4 KB of memory */
u32 commit_page_granularity = 64; /* every time we need to commit more memory, commit in chunks of 64 pages */
ArenaFlags flags = 
    ArenaFlags_Decommit |         /* when popping/clearing, decommit unused pages */
    ArenaFlags_CommitChunked |    /* when allocating requires more memory, commit the number of pages defined above */
    ArenaFlags_DebugFillOnClear;  /* when popping/clearing, fill any remaining committed memory with 0xDD */

/* reserve 64 MB,  */
Arena arena = arena_alloc_ex(MB(64), initial_commit_size, commit_page_granularity, flags);
```

### Allocation

Use `arena_push` for explicit control:

```c
/* declared in aether/aether.h */
void* arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero);

/* push to arena `size` bytes, padded to `alignment`, and explicitly zero the memory */
void* memory = arena_push(&arena, size, alignment, ArenaZero_Force);
```

 Convenience macros are provided for common typed and array allocations. Default alignment is an 8-byte boundary. The `zero` argument is an `ArenaZero`, not a plain bool:
 - `ArenaZero_FollowPolicy` (the default macros) — zeroed only if `ArenaFlags_AlwaysZero` is set on the arena (the default in debug builds).
 - `ArenaZero_Force` (the `_zero` variants) — always zeroed, regardless of policy.
 - `ArenaZero_Never` (the `_nozero` variants) — never zeroed, even if `ArenaFlags_AlwaysZero` is set.

```c
/* push one u32 onto the arena */
u32* one   = arena_push_t(&arena, u32);
/* push and array of f64 onto the arena, with space for 1024 elements */
f64* array = arena_push_array(&arena, f64, 1024);
```

```c
/* explicitly request zeroing for this allocation, regardless of arena flags */
u32* one_zeroed   = arena_push_t_zero(&arena, u32);
f64* array_zeroed = arena_push_array_zero(&arena, f64, 1024);
```

```c
/* explicitly skip zeroing for this allocation, even if ArenaFlags_AlwaysZero is set */
u8* bytes = arena_push_array_nozero(&arena, u8, 256);
```

> [!NOTE] 
>  Note that newly committed pages may still be zeroed by the operating system

### Lifetime Model

AETHER arenas are designed for bulk lifetime management:

```c
/* save the position for later */
u64 mark = arena.pos;

TemporaryThing *tmp = arena_push_t(&arena, TemporaryThing);

/*--------------------------------------------------------*/
/* use temporary allocations */
/*--------------------------------------------------------*/

arena_pop_to(&arena, mark);
```

***Common Patterns***

| PATTERN         | DESCRIPTION                                                           |
| --------------- | --------------------------------------------------------------------- |
| Permament Arena | holds memory for lifetime of program, e.g. for application state/data |
| Frame Arena     | Clear once per frame or iteration, should not decommit                |
| Scratch Arena   | Save `pos`, allocate temparary data, then pop back                    |

## Use Case: Temporary File Processing

A common pattern is to use a scratch arena for temporary data that only needs to live for the duration of a task. `arena_read_file` reads an entire file into the arena in one call and returns a `bytes` view (`{ u8* data; u64 size; }`).

> `arena_alloc(MB(64))` reserves 64 MB of virtual address space. Initial commit, zeroing, and decommit behavior follow the build-configuration defaults described above.

```c
Arena scratch = arena_alloc(MB(64));

/*--------------------------------------------------------*/
/* Other allocations/deallocations on scratch             */
/*--------------------------------------------------------*/

ArenaTemp tmp = arena_begin_temp(&scratch);

/* Read the whole file into the arena. On a missing or
   unreadable file the result is {0} and the arena is left
   untouched, so the caller can recover. */
bytes file = arena_read_file(&scratch, "input.dat");

/*--------------------------------------------------------*/
/* Process file contents                                  */
/*--------------------------------------------------------*/

if (file.data)
{
    process_file(file.data, file.size);
}
else
{
    /* nothing was pushed onto the arena, so there is nothing
       to unwind here — report and carry on */
    fprintf(stderr, "could not read input.dat\n");
}

/*--------------------------------------------------------*/
/* Discard all temporary allocations at once              */
/*--------------------------------------------------------*/

arena_end_temp(tmp);   /* file bytes reclaimed with everything else */

/*--------------------------------------------------------*/
/* continue to re-use scratch arena for other temp work   */
/*--------------------------------------------------------*/

arena_release(&scratch);
```

No individual deallocation is required. The file bytes live in the arena, so once processing is complete the entire working set is discarded together by ending the `ArenaTemp` (or with `arena_pop_to` / `arena_clear`).

### Read-only files: `map_file`

When a file is large and read-only, `map_file` maps it directly into the address space instead of copying it into an arena. It returns a `bytes_view` (`const u8*`) whose pages are backed by the OS and faulted in on demand.

```c
bytes_view file = map_file("big.dat");   /* OS-backed pages, not arena memory */
if (file.data)
{
    process_file_ro(file.data, file.size);
    unmap_file(file);                    /* independent of any arena lifetime */
}
```

A mapped view is **not** owned by any arena: `arena_pop_to` / `arena_clear` do not release it, and it must be returned explicitly with `unmap_file`. Use `arena_read_file` when you need mutable bytes or want the data to share the arena's lifetime; use `map_file` for large read-only inputs you want to page in lazily.

## Use Case: Nested Scratch Arenas in a Numerical Kernel

`ArenaTemp` (`arena_begin_temp` / `arena_end_temp`) names the mark/pop_to pattern above, and nests cleanly: an outer computation can hold its own scratch buffers while calling inner steps that need their own short-lived ones — e.g. an RK4 time step, where the four stage-derivative buffers must outlive each individual flux evaluation.

```c
static void eval_rhs(const f64* state, f64* out_rhs, u64 n, f64 dx, Arena* scratch)
{
    ArenaTemp temp = arena_begin_temp(scratch);   /* inner: this call only */

    f64* flux = arena_push_array(scratch, f64, n + 1);
    compute_flux(state, flux, n);

    for (u64 i = 0; i < n; ++i)
        out_rhs[i] = -(flux[i + 1] - flux[i]) / dx;

    arena_end_temp(temp);                         /* flux buffer reclaimed */
}

void rk4_step(f64* y, u64 n, f64 dt, f64 dx, Arena* scratch)
{
    ArenaTemp step = arena_begin_temp(scratch);   /* outer: whole step */

    f64* k1  = arena_push_array(scratch, f64, n);
    f64* k2  = arena_push_array(scratch, f64, n);
    f64* k3  = arena_push_array(scratch, f64, n);
    f64* k4  = arena_push_array(scratch, f64, n);
    f64* tmp = arena_push_array(scratch, f64, n);

    eval_rhs(y, k1, n, dx, scratch);
    for (u64 i = 0; i < n; ++i) tmp[i] = y[i] + 0.5*dt*k1[i];
    eval_rhs(tmp, k2, n, dx, scratch);
    for (u64 i = 0; i < n; ++i) tmp[i] = y[i] + 0.5*dt*k2[i];
    eval_rhs(tmp, k3, n, dx, scratch);
    for (u64 i = 0; i < n; ++i) tmp[i] = y[i] + dt*k3[i];
    eval_rhs(tmp, k4, n, dx, scratch);

    for (u64 i = 0; i < n; ++i)
        y[i] += (dt/6.0) * (k1[i] + 2*k2[i] + 2*k3[i] + k4[i]);

    arena_end_temp(step);                         /* k1..k4, tmp all reclaimed together */
}
```

Each call to `eval_rhs` reuses the same bytes for `flux`, four times per step, without ever disturbing `k1`..`k4` — the outer `ArenaTemp` only unwinds once, after the full step. Allocate `scratch` once outside the time-stepping loop; no allocator calls happen inside it.

## Use Case: String allocations

```c
#include "aether/aether.h"

void string_example(void)
{
    /* Reserve 64 MB; debug vs release behavior follows arena_alloc defaults. */
    Arena arena = arena_alloc(MB(64));

    /*----------------------------------------------*/
    /* Basic arena-backed strings                   */
    /*----------------------------------------------*/

    /* Literal to str8_view then copy into the arena */
    str8_view src = STR("hello, world");
    str8      s   = arena_push_str8_copy(&arena, src);

    /* Print using STR8_FMT / STR8_ARG */
    printf("s = " STR8_FMT "\n", STR8_ARG(s));

    /*----------------------------------------------*/
    /* Formatted strings (str8 and C strings)       */
    /*----------------------------------------------*/

    /* Create a formatted str8 backed by the arena */
    str8 message = arena_push_str8_fmt(
        &arena,
        "name= " STR8_FMT ", value=%d",
        STR8_ARG(s),
        42
    );

    printf("message = " STR8_FMT "\n", STR8_ARG(message));

    /* Create a formatted null-terminated C string on the arena */
    char* line = arena_push_cstring_fmt(&arena, "count = %d", 123);
    printf("line = %s\n", line);

    /*----------------------------------------------*/
    /* Lifetime management                           */
    /*----------------------------------------------*/

    /* When done with these strings, clear or pop the arena */
    arena_clear(&arena);   /* or arena_pop_to(&arena, mark) if you saved a mark */
    arena_release(&arena); /* release the reserved region back to the OS */
}
```

## Ring Buffers

AETHER provides a fixed-capacity **ring (circular) buffer** for byte streams — a single-producer / single-consumer FIFO useful for pipes, logging, and telemetry.

It uses the *virtually-mirrored* (a.k.a. "magic") layout: the buffer reserves twice its capacity of address space and maps the **same physical pages** into both halves. A read or write that crosses the end of the buffer therefore wraps automatically, so any span up to the full capacity is always a **single `memcpy`** and `ring_buffer_peek` can hand back **one contiguous view even when the data straddles the seam** — no split-at-the-boundary bookkeeping.

> [!NOTE]
> Ring buffers are currently Windows-only and require Windows 10 version 1803 or newer (`VirtualAlloc2` / `MapViewOfFile3`). These are resolved at runtime, so there is **no extra link dependency** — on an older OS `ring_buffer_alloc` simply returns `false`. Always check its return value.

The requested capacity is rounded up to a power of two that is at least the OS allocation granularity (64 KiB on Windows). This keeps index wrapping a cheap mask and satisfies the placeholder-mapping alignment rules.

### Layout

| FIELD | DESCRIPTION                                                      |
| ----- | ---------------------------------------------------------------- |
| base  | Base of the lower mapped view (the mirror lives at `base + size`) |
| size  | Capacity in bytes (power of two ≥ allocation granularity)        |
| read  | Monotonically increasing read cursor (masked by `size` on access) |
| write | Monotonically increasing write cursor (masked by `size` on access) |

`read` and `write` are ever-increasing counters; bytes available to read is simply `write - read`, and free space is `size - (write - read)`. They are masked to an index only at the moment of access, which keeps "full" and "empty" unambiguous and stays correct across 64-bit overflow.

### API

| FUNCTION              | DESCRIPTION                                                                                  |
| --------------------- | -------------------------------------------------------------------------------------------- |
| `ring_buffer_alloc`        | Reserve + map the mirrored region. Returns `false` on failure (incl. pre-1803 Windows).      |
| `ring_buffer_available`    | Bytes currently ready to read (`write - read`).                                              |
| `ring_buffer_write`        | Copy `len` bytes in. Rejects (returns `false`) if `len` exceeds free space or total capacity. |
| `ring_buffer_peek`         | Return a contiguous `bytes_view` of `len` bytes **without** consuming. `{0}` if `len` unavailable. |
| `ring_buffer_read`         | Copy `len` bytes out and advance the read cursor. Returns `false` if `len` unavailable.       |
| `ring_buffer_advance_read` | Advance the read cursor by `len` **without** copying (zero-copy consume). `false` if `len` exceeds available. |
| `ring_buffer_release`      | Unmap both views, free the reservation, and zero the struct.                                 |

```c
RingBuffer rb = {0};
if (!ring_buffer_alloc(&rb, KB(64)))   /* rounded up to a power of two >= 64 KiB */
{
    /* unavailable (e.g. Windows < 1803) — fall back or bail */
    return;
}

/* producer: write the whole span or nothing.
   returns false if there isn't room — apply backpressure or drop */
u8 frame[1024];
if (!ring_buffer_write(&rb, frame, sizeof(frame)))
{
    /* buffer full */
}

/* consumer: inspect before committing to a read */
bytes_view head = ring_buffer_peek(&rb, sizeof(frame));
if (head.size)
{
    /* head.data is ONE contiguous span even when the logical data wraps
       the end of the buffer — the mirror makes the seam invisible */
    parse(head.data, head.size);

    u8 sink[1024];
    ring_buffer_read(&rb, sink, head.size);   /* now advance the read cursor */
}

ring_buffer_release(&rb);                      /* unmap + zero the struct */
```

### Zero-copy consume: `peek` → `advance_read`

For a sink that can read straight from the buffer — e.g. a socket `send()` — skip the copy entirely: `peek` a contiguous span, hand its pointer to the consumer, then advance the read cursor by however much was actually consumed.

```c
/* point the socket straight at the buffer; no intermediate copy */
bytes_view v = ring_buffer_peek(&rb, ring_buffer_available(&rb));
if (v.size)
{
    i64 n = send(sock, (const char*)v.data, (int)v.size, 0);  /* may accept < v.size */
    if (n > 0)
        ring_buffer_advance_read(&rb, (u64)n);                /* advance by what was sent */
}
```

Two things make this safe and convenient:

- **One `send()` covers wrapped data.** `peek` returns a *single contiguous* span even when the logical bytes straddle the seam (the mirror), so there is never any need for scatter/gather (`iovec`) to handle the wrap.
- **The span stays valid for the whole call.** The producer can never write past the read cursor, and you do not advance `read` until *after* `send()` returns — so the producer cannot overwrite the in-flight bytes. Advancing by the returned count (not by what you peeked) also handles partial sends correctly.

> [!NOTE]
> The buffer is **not** thread-safe on its own. It is sized for a single producer and single consumer; concurrent access needs external synchronization (or the lock-free SPSC discipline of one writer touching `write` and one reader touching `read`).
