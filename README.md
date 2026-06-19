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

```c
/* reserve 64 MB, and initially commit 64 KB */
Arena arena = arena_alloc(MB(64), KB(64));

/* push an array of 128 u32 values, and zero the memory*/
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

| FIELD         | DESCRIPTION                                        |
| -----         | -------------------------------------------------- |
| base          | Base pointer to the reserved memory region         |
| reserved_size | Total virtual memory reserved for the arena        |
| commit_size   | Amount of memory currently commited and accessible |
| pos           | Current allocation position in bytes from `base`   |
| flags         | 8-bit flag for controlling arena behaviour         |

Allocation occur from `base + pos`. The position is advanced after each push.

### Arena Flags

`ArenaFlags` is currently stored as a `u8`. This allows up to 8 independent flags. The type can be widened later if needed. 

| FLAG                       | BIT SET  | DESCRIPTION                                                   |
| -------------------------- | -------- | ------------------------------------------------------------- |
| `ArenaFlags_None`          | `0`      | No special behaviour                                          |
| `ArenaFlags_Decommit`      | `BIT(0)` | Decommit unused tail memory pages when popping or clearning   |
| `ArenaFlags_CommitChunked` | `BIT(1)` | Commit memory in larger chunks (64 pages)                     |
| `ArenaFlags_AlwaysZero`    | `BIT(2)` | Zero memory for every allocation, regardless of the push call |
| `ArenaFlags_DebugFillOnClear`    | `BIT(3)` | fill invalidated memory with `0xDD` when popping/clearing |

***Example***

```c
/* reserve 64 MB but commit nothing initially */
Arena arena = arena_alloc(MB(64), 0);

arena.flags =
    ArenaFlags_Decommit |         /* when popping/clearing, decommit unused pages */
    ArenaFlags_CommitChunked |    /* when allocating, commit 64 pages at a time */
    ArenaFlags_DebugFillOnClear;  /* when popping/clearning, fill any remaining committed memory with 0xDD */
```

### Allocation

Use `arena_push` for explicit control:

```c
/* declared in aether/aether.h */
void arena_push(Arena* arena, u64 size, u64 align, b8 zero);

/* push to arena `size` bytes, padded to `alignment`, and explicitly zero the memory */
void* memory = arena_push(&arena, size, alignment, true);
```

Convenience macros are provided for common typed and array allocations. Default alignment is 8-bytes, and the default behaviour is to zero the allocated memory. 

```c
/* push one u32 onto the arena */
u32* one   = arena_push_t(&arena, u32);
/* push and array of f64 onto the arena, with space for 1024 elements */
f64* array = arena_push_array(&arena, f64, 1024);
```

No-zero variants skip the explicit zero-fill:

```c
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

/* use temporary allocations */

arena_pop_to(&arena, mark);
```

***Common Patterns***

| PATTERN         | DESCRIPTION                                                           |
| --------------- | --------------------------------------------------------------------- |
| Permament Arena | holds memory for lifetime of program, e.g. for application state/data |
| Frame Arena     | Clear once per frame or iteration, should not decommit                |
| Scratch Arena   | Save `pos`, allocate temparary data, then pop back                    |

