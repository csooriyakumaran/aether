# AETHER

***the underlying medium from which all else emerges ..***

AETHER is a small single-header core library for personal c/c++ projects. 

It provides a minimal set of common primitives:

- fixed-width type aliases
- assertions and utility macros
- string views
- bit flags
- memory arenas

The intent is not to be a framework, but rather a lightweight foundation that can be included in other libraries or applications. The motivation is to share primitives between different aerodynamic solver codes.

## Memory Arenas

AETHER provides a simple linear arena allocator. 

An arena reserves a block of virtual address space and commits memory as needed. Allocations are performed by bumping a position pointer forward. Individual allocations are not freed; instead, memory is in bulk by popping, clearing, or releasing the arena. 

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
u32* values = arena_push_array(&arena, u32, 128)

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

A common pattern is to use a scratch arena for temporary data that only needs to live for the duration of a task.

> `arena_alloc(MB(64))` reserves 64 MB of virtual address space. Initial commit, zeroing, and decommit behavior follow the build-configuration defaults described above.

```c
Arena scratch = arena_alloc(MB(64));

/*--------------------------------------------------------*/
/* Other allocations/deallocations on scratch */
/*--------------------------------------------------------*/

u64 mark = scratch.pos;

FILE* fp = fopen("input.dat", "rb");
ASSERT(fp);

fseek(fp, 0, SEEK_END);
u64 file_size = (u64)ftell(fp);
fseek(fp, 0, SEEK_SET);

u8* file_data = arena_push_array(&scratch, u8, file_size);

u64 bytes_read = fread(file_data, 1, file_size, fp);
ASSERT(bytes_read == file_size);

fclose(fp);

/*--------------------------------------------------------*/
/* Process file contents                                  */
/*--------------------------------------------------------*/

process_file(file_data, file_size);

/*--------------------------------------------------------*/
/* Discard all temporary allocations at once              */
/*--------------------------------------------------------*/

arena_pop_to(&scratch, mark);
/* or if other temporaries are no longer needed... */
arena_clear(&scratch);

/*--------------------------------------------------------*/
/* continue to re-use scratch arena for other temp work */
/*--------------------------------------------------------*/

arena_release(&scratch);

```

No individual deallocation is required. Once processing is complete, the entire working set is discarded with a single call to `arena_pop_to` or `arena_clear`.


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

    /* Literal to cstr8 then copy into the arena */
    cstr8 src  = STR("hello, world");
    str8  s    = arena_push_str8_copy(&arena, src);

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
