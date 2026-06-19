/*---------------------------------------------------------------------------*\
  AETHER

  Minimal Core Library for C/C++

  Author      : Christopher Sooriyakumaran
  Created     : 2026-06-18
  License     : MIT

  Core types, memory arenas, spans, assertions, and utility primitives.
\*---------------------------------------------------------------------------*/
#ifndef AETHER_H_
#define AETHER_H_

#include "aether/aether-version.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*-------- C O N F I G -------------------------------------------------------*/

#if defined(_MSC_VER)
    #define DEBUG_BREAK() __debugbreak()
#else 
    #define DEBUG_BREAK() __builtin_trap()
#endif // _MSC_VER

/*----------------------------------------------------------------------------*/

#ifndef AETHER_ENABLE_ASSERTS
    #if defined(_DEBUG) || !defined(NDEBUG)
        #define AETHER_ENABLE_ASSERTS 1
    #else
        #define AETHER_ENABLE_ASSERTS 0
    #endif
#endif // AETHER_ENABLE_ASSERTS

#if AETHER_ENABLE_ASSERTS
    #include <stdio.h>
    #define ASSERT(x) do {                                                                      \
        if (!(x)) {                                                                             \
            fprintf(stderr, "ASSERT FAIED: %s [\%s:%d]\n", #x, __FILE__, __LINE__); \
            DEBUG_BREAK();                                                                      \
        }                                                                                       \
    } while (0)
#else
    #define ASSERT(x) ((void)0)
#endif // AETHER_ENABLE_ASSERTS


#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*-------- T Y P E S ---------------------------------------------------------*/

typedef bool     b8;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

/*-------- S T R I N G S -----------------------------------------------------*/

typedef struct str8  {       u8* data; u64 size; } str8;
typedef struct cstr8 { const u8* data; u64 size; } cstr8;

#define STR(s) ((cstr8){ (const u8*)(s), sizeof(s) - 1 })
#define STR8_FMT "%.*s"
#define STR8_ARG(s) ((int)((s).size)), ((const char*)((s).data))

/*----------------------------------------------------------------------------*/

#define BIT_U8(x)  ((u8)  (1u   << (x)))
#define BIT_U16(x) ((u16) (1u   << (x)))
#define BIT_U32(x) ((u32) (1u   << (x)))
#define BIT_U64(x) ((u64) (1ull << (x)))

#define BIT(x) BIT_U64(x)

#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)
#define TB(n) (((u64)(n)) << 40)

/*-------- A R E N A S -------------------------------------------------------*/

#define ARENA_DEFAULT_ALIGNMENT sizeof(void*)
#define ARENA_DEFAULT_COMMIT_PAGES 64

typedef u8 ArenaFlags;
enum ArenaFlags_
{
    ArenaFlags_None             = 0u,
    ArenaFlags_Decommit         = BIT(0), /* Decommit memory when popping/clearing arena    */
    ArenaFlags_CommitChunked    = BIT(1), /* Only commit a page count of ARENA_DEFAULT_COMMIT_PAGES */
    ArenaFlags_AlwaysZero       = BIT(2), /* Always zero memory                             */
    ArenaFlags_DebugFillOnClear = BIT(3)  /* On clear, set bytes to 0xDD for debugging      */
};

typedef struct Arena
{
    u8* base;
    u64 reserved_size;
    u64 commit_size;
    u64 pos;

    ArenaFlags flags;
} Arena;

Arena arena_alloc(u64 reserve_size, u64 initial_commit_size);
void  arena_release(Arena* arena);

void* arena_push(Arena* arena, u64 size, u64 align, b8 zero);
void  arena_pop(Arena* arena, u64 amt);
void  arena_pop_to(Arena* arena, u64 pos);

void  arena_clear(Arena* arena);

#define arena_push_t(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_DEFAULT_ALIGNMENT, 1)
#define arena_push_t_nozero(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_DEFAULT_ALIGNMENT, 0)

#define arena_push_array(arena, T, n) (T*)arena_push((arena), sizeof(T) * (n), ARENA_DEFAULT_ALIGNMENT, 1)
#define arena_push_array_nozero(arena, T, n) (T*)arena_push((arena), sizeof(T) * (n), ARENA_DEFAULT_ALIGNMENT, 0)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // AETHER_H_


/*---------------------------------------------------------------------------*/
#ifdef AETHER_IMPLEMENTATION

#include <string.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif // WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif // NOMINMAX

    #include <windows.h>

#endif // _WIN32


static u64 os_mem_pagesize(void)
{
    static u64 pagesize = 0;

    if (pagesize == 0) 
    {
#ifdef _WIN32
        SYSTEM_INFO sysinfo = {0};
        GetSystemInfo(&sysinfo);
        pagesize = (u64)sysinfo.dwPageSize; 
#else
        #error "AETHER: OS memory page size not implemented for this platform"
#endif
    }

    return pagesize;
}

static void* os_mem_reserve(u64 size)
{
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
#else
    #error "AETHER: OS memory allocation not implemented for this platform"
#endif
}

static b8 os_mem_commit(void* ptr, u64 size)
{
#ifdef _WIN32
    void* ret = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    return ret != NULL;
#else
    #error "AETHER: OS memory commit not implemented for this platform"
#endif
}

static b8 os_mem_decommit(void* ptr, u64 size)
{
#ifdef _WIN32
    return VirtualFree(ptr, size, MEM_DECOMMIT);
#else
    #error "AETHER: OS memory decommit not implemented for this platform"
#endif
}

static b8 os_mem_release(void* ptr)
{
#ifdef _WIN32
    return VirtualFree(ptr, 0, MEM_RELEASE);
#else
    #error "AETHER: OS memory release not implemented for this platform"
#endif
}

static u64 align_forward_u64(u64 value, u64 align)
{
    ASSERT(align != 0);
    ASSERT((align & (align - 1)) == 0);

    u64 mask = align - 1;
    return (value + mask) & ~mask;
}


static void arena_commit_page_or_chunk(Arena* arena, u64 new_pos)
{
    if (new_pos <= arena->commit_size)
        return;

    u64 new_commit_size;
    u64 pagesize = os_mem_pagesize();

    if ((arena->flags & ArenaFlags_CommitChunked) != 0)
        new_commit_size = align_forward_u64(new_pos, pagesize * ARENA_DEFAULT_COMMIT_PAGES);
    else
        new_commit_size = align_forward_u64(new_pos, pagesize);

    new_commit_size = MIN(new_commit_size, arena->reserved_size);
        
    b8 committed = os_mem_commit(arena->base + arena->commit_size, new_commit_size - arena->commit_size);
    ASSERT(committed);

    arena->commit_size = new_commit_size;

}


static void arena_decommit_tail(Arena* arena, u64 new_pos)
{
    if ((arena->flags & ArenaFlags_Decommit) == 0)
        return;

    u64 pagesize = os_mem_pagesize();
    u64 new_commit_size = align_forward_u64(new_pos, pagesize);

    // We don't have a tail
    if (new_commit_size > arena->commit_size)
        return;


    u64 decommit_size = arena->commit_size - new_commit_size;

    if (decommit_size > 0)
    {
        os_mem_decommit(arena->base + new_commit_size, decommit_size);
        arena->commit_size = new_commit_size;
    }

    return;
}


Arena arena_alloc(u64 reserve_size, u64 initial_commit_size)
{
    ASSERT(reserve_size > 0);

    Arena arena = {0};

    u64 pagesize = os_mem_pagesize();

    reserve_size = align_forward_u64(reserve_size, pagesize);
    initial_commit_size  = align_forward_u64(initial_commit_size, pagesize);

    if (initial_commit_size > reserve_size)
        initial_commit_size = reserve_size;

    arena.base   = (u8*)os_mem_reserve(reserve_size);
    ASSERT(arena.base);

    if (initial_commit_size > 0)
    {
        b8 committed = os_mem_commit(arena.base, initial_commit_size);
        ASSERT(committed);
    }

    arena.reserved_size = reserve_size;
    arena.commit_size   = initial_commit_size;
    arena.pos           = 0;

    return arena;
}


void arena_release(Arena* arena)
{
    if (arena->base)
        os_mem_release(arena->base);

    arena->base          = 0;
    arena->reserved_size = 0;
    arena->commit_size   = 0;
    arena->pos           = 0;
}


void* arena_push(Arena* arena, u64 size, u64 align, b8 zero)
{
    u64 aligned_pos = align_forward_u64(arena->pos, align);
    u64 new_pos     = aligned_pos + size;

    ASSERT(new_pos <= arena->reserved_size);

    if (new_pos > arena->commit_size)
        arena_commit_page_or_chunk(arena, new_pos);

    arena->pos = new_pos;
    void* result = arena->base + aligned_pos;

    // - memset to zero if we explicity ask, or if AlwaysZero Policy is set
    if (zero || (arena->flags & ArenaFlags_AlwaysZero) != 0)
        memset(result, 0, size);

    return result;

}

void arena_pop_to(Arena* arena, u64 pos)
{
    if (pos > arena->pos)
        pos = arena->pos;

    if ((arena->flags & ArenaFlags_DebugFillOnClear) != 0)
        memset(arena->base + pos, 0xDD, arena->pos - pos);

    if ((arena->flags & ArenaFlags_Decommit) != 0)
        arena_decommit_tail(arena, pos);

    arena->pos = pos;
}


void arena_pop(Arena* arena, u64 amt)
{
    if(amt > arena->pos)
        amt = arena->pos;

    arena_pop_to(arena, arena->pos - amt);
}

void arena_clear(Arena* arena)
{
    arena_pop_to(arena, 0);
}


#endif // AETHER_IMPLEMENTATION
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*\
   revision history:
   ----------------

      v0.0.1 (2026-06-18)
      - first released version

\*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*\
   LICENSE
   -------

   Copyright 2026 C. Sooriyakumaran

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the “Software”), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell co-
   pies of the Software, and to permit persons to whom the Software is furnished
   to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
   NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUT-
   HORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

\*---------------------------------------------------------------------------*/
