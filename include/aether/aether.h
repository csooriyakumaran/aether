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
#include <stdio.h>
#include <stdlib.h>
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

#if defined(__cplusplus)
    #define ARENA_ALIGN(T) alignof(T)
#else
    #define ARENA_ALIGN(T) _Alignof(T)
#endif // __cplusplus


/*----------------------------------------------------------------------------*/

#ifndef AETHER_ENABLE_ASSERTS
    #if defined(_DEBUG) || !defined(NDEBUG)
        #define AETHER_ENABLE_ASSERTS 1
    #else
        #define AETHER_ENABLE_ASSERTS 0
    #endif
#endif // AETHER_ENABLE_ASSERTS

#if AETHER_ENABLE_ASSERTS
    #define ASSERT(x) do {                                                                      \
        if (!(x)) {                                                                             \
            fprintf(stderr, "ASSERT FAILED: %s [%s:%d]\n", #x, __FILE__, __LINE__); \
            DEBUG_BREAK();                                                                      \
        }                                                                                       \
    } while (0)
#else
    #define ASSERT(x) ((void)0)
#endif // AETHER_ENABLE_ASSERTS

#define FATAL(msg) do {  \
    fprintf(stderr, "FATAL ERROR: %s [%s:%d]\n", msg, __FILE__, __LINE__); \
    DEBUG_BREAK(); abort(); \
} while(0)

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

#define BIT8(x)  ((u8)  (1u   << (x)))
#define BIT16(x) ((u16) (1u   << (x)))
#define BIT32(x) ((u32) (1u   << (x)))
#define BIT64(x) ((u64) (1ull << (x)))

#define BIT(x) BIT64(x)

#define KB(n) (((u64)(n)) << 10)
#define MB(n) (((u64)(n)) << 20)
#define GB(n) (((u64)(n)) << 30)
#define TB(n) (((u64)(n)) << 40)

/*-------- A R E N A S -------------------------------------------------------*/

// todo(chris): ArenaFlags may need to grow to u16 or u32 if other options are added
typedef u8 ArenaFlags;
enum ArenaFlags_ {
    ArenaFlags_None             = 0u,
    ArenaFlags_Decommit         = BIT8(0), /* Decommit memory when popping/clearing arena    */
    ArenaFlags_CommitChunked    = BIT8(1), /* Only commit a page count set by granularity field */
    ArenaFlags_AlwaysZero       = BIT8(2), /* Always zero memory                             */
    ArenaFlags_DebugFillOnClear = BIT8(3)  /* On clear, set bytes to 0xDD for debugging      */
};

typedef enum ArenaZero {
    ArenaZero_FollowPolicy = 0,
    ArenaZero_Force        = 1,
    ArenaZero_Never        = 2
} ArenaZero;

typedef struct Arena {
    u8* base;
    u64 reserved_size;
    u64 commit_size;
    u64 pos;

    u32 granularity;
    ArenaFlags flags;
} Arena;

Arena arena_alloc_ex(u64 reserve_size, u64 initial_commit_size, u32 commit_page_granularity, ArenaFlags flags);
Arena arena_alloc(u64 reserve_size);
void  arena_release(Arena* arena);

void* arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero);
void  arena_pop(Arena* arena, u64 amt);
void  arena_pop_to(Arena* arena, u64 pos);

void  arena_clear(Arena* arena);

// default arena_push will respect zeroing policy controlled by ArenaFlags
#define arena_push_t(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_ALIGN(T), ArenaZero_FollowPolicy)
#define arena_push_array(arena, T, n) (T*)arena_push((arena), sizeof(T) * (n), ARENA_ALIGN(T), ArenaZero_FollowPolicy)

// explicity force zeroing after push regargless of ArenaFlags
#define arena_push_t_zero(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_ALIGN(T), ArenaZero_Force)
#define arena_push_array_zero(arena, T, n) (T*)arena_push((arena), sizeof(T) * (n), ARENA_ALIGN(T), ArenaZero_Force)

// todo(chris): figure out the semantics
// explicity force no-zeroing after push regargless of ArenaFlags
#define arena_push_t_nozero(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_ALIGN(T), ArenaZero_Never)
#define arena_push_array_nozero(arena, T, n) (T*)arena_push((arena), sizeof(T) * (n), ARENA_ALIGN(T), ArenaZero_Never)

// strings on the arena
char* arena_push_cstring(Arena* arena, const char* src);
char* arena_push_cstring_fmt(Arena* arena, const char* fmt, ...);

str8  arena_push_str8_copy(Arena* arena, cstr8 src);
str8  arena_push_str8_from_cstring(Arena*, const char* src);
str8  arena_push_str8_fmt(Arena* arena, const char* fmt, ...);

// file i/o
str8  arena_read_file(Arena* arena, const char* path);

/*-------- M E M - M A P P I N G  -------------------------------------------*/

str8  map_file(const char* path);
void  unmap_file(str8 map);

/*-------- H E L P E R S ----------------------------------------------------*/
u64 time_mark(void);
f64 time_elapsed_sec(u64 start, u64 end);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // AETHER_H_


/*---------------------------------------------------------------------------*/
#ifdef AETHER_IMPLEMENTATION

#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif // WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif // NOMINMAX

    #include <windows.h>

#endif // _WIN32

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

static u64 os_mem_pagesize(void)
{

#ifdef _WIN32
        SYSTEM_INFO sysinfo = {0};
        GetSystemInfo(&sysinfo);
        u64 pagesize = (u64)sysinfo.dwPageSize; 
        return pagesize;
#else
        #error "AETHER: OS memory page size not implemented for this platform"
#endif

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

/* todo(chris): maybe we need an os_file_open_ex,
 *              or os_file_open that allows user
 *              defined read/write mode */

/* ... */

static void* os_file_open_for_read(const char* path)
{
#ifdef _WIN32
    HANDLE h = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    return h;
#else
    #error "AETHER: OS file open for read not implemented for this platform"
#endif
}

static void* os_file_open_for_write(const char* path)
{
#ifdef _WIN32
    return NULL;
#else
    #error "AETHER: OS file open for write not implemented for this platform"
#endif
}

static void os_file_close(void* handle)
{
#ifdef _WIN32
    CloseHandle(handle);
#else
    #error "AETHER: OS file close not implemented for this platform"
#endif
}

static b8  os_file_size(void* handle, u64* out_size)
{
#ifdef _WIN32

    LARGE_INTEGER filesize;
    if(!GetFileSizeEx(handle, &filesize))
        return false;

    *out_size = (u64)filesize.QuadPart;
    return true;
#else
    #error "AETHER: OS file size not implemented for this platform"
#endif
}

static b8  os_file_read(void* handle, void* dst, u64 size)
{
#ifdef _WIN32

    u8* cursor = (u8*)dst;
    u64 remaining = size;

    while(remaining >0)
    {
        /* Note(Chris):
         * - capping at MAXWORD (~4 GB) may be exessive
         * - could reduce in future to 1 GB */
        DWORD chunk = (remaining  > MAXDWORD) ? MAXDWORD : (DWORD)remaining;
        DWORD bytes_read = 0;

        if (!ReadFile(handle, cursor, chunk, &bytes_read, NULL))
            return false;

        if (bytes_read == 0) /* EOF before reaching reqested size */
            return false;

        cursor    += bytes_read;
        remaining -= bytes_read;
    }

    return true;
#else
    #error "AETHER: OS file read not implemented for this platform"
#endif
}

static void* os_file_map(void* handle, u64 size)
{
#ifdef _WIN32
    (void)size;
    HANDLE hmap = CreateFileMapping(handle, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hmap) {return NULL; }
    void* data = MapViewOfFile(hmap, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(hmap);
    return data;
#else
    #error "AETHER: Os file map not implemented on this platform"
#endif
}

static void os_file_unmap(void* ptr, u64 size)
{
#ifdef _WIN32
    (void)size;
    UnmapViewOfFile(ptr);
#else
    #error "AETHER: Os file unmap not implemented on this platform"
#endif
}

static u64 os_time_now(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (u64)counter.QuadPart;
#else
    #error "AETHER: OS time now not implemented for this platform"
#endif
}

static u64 os_time_frequency(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (u64)freq.QuadPart;
#else
    #error "AETHER: OS time frequency not implemented for this platform"
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
    {
        u32 page_granularity = arena->granularity > 0 ? arena->granularity : 1;
        new_commit_size = align_forward_u64(new_pos, pagesize * page_granularity);
    } else {
        new_commit_size = align_forward_u64(new_pos, pagesize);
    }

    new_commit_size = MIN(new_commit_size, arena->reserved_size);
        
    b8 committed = os_mem_commit(arena->base + arena->commit_size, new_commit_size - arena->commit_size);
    if(!committed) FATAL("Memory commit faild");

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

Arena arena_alloc_ex(u64 reserve_size, u64 initial_commit_size, u32 commit_page_granularity, ArenaFlags flags)
{
    ASSERT(reserve_size > 0);

    Arena arena = {0};

    u64 pagesize = os_mem_pagesize();

    reserve_size = align_forward_u64(reserve_size, pagesize);
    initial_commit_size  = align_forward_u64(initial_commit_size, pagesize);

    if (initial_commit_size > reserve_size)
        initial_commit_size = reserve_size;

    arena.base   = (u8*)os_mem_reserve(reserve_size);
    if (!arena.base) FATAL("Failed to reserve memory");

    if (initial_commit_size > 0)
    {
        b8 committed = os_mem_commit(arena.base, initial_commit_size);
        if(!committed) FATAL("Failed to commit memory");
    }

    arena.reserved_size = reserve_size;
    arena.commit_size   = initial_commit_size;
    arena.pos           = 0;
    arena.flags         = flags;
    arena.granularity   = commit_page_granularity;

    return arena;

}

Arena arena_alloc(u64 reserve_size)
{
#if AETHER_ENABLE_ASSERTS
    // Debug defaults
    u64        initial_commit = 0;
    ArenaFlags flags          = ArenaFlags_AlwaysZero | 
                                ArenaFlags_DebugFillOnClear |
                                ArenaFlags_Decommit;
    u32 granularity           = 1; /* commit and decommit single-page units */
#else 
    // Performance defaults
    u64        initial_commit = 0;
    ArenaFlags flags          = ArenaFlags_None;
    u32        granularity    = 1;
#endif
    return arena_alloc_ex(reserve_size, initial_commit, granularity, flags);

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


void* arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero)
{
    ASSERT(arena->pos <= arena->reserved_size);

    u64 aligned_pos = align_forward_u64(arena->pos, align);
    ASSERT(aligned_pos <= arena->reserved_size);

    // catch u64 overflow
    if (size > arena->reserved_size - aligned_pos)
    {
        ASSERT(!"arena_push overflow: allocation exceeds reserved size");
        return NULL;
    }

    u64 new_pos = aligned_pos + size;
    ASSERT(new_pos <= arena->reserved_size);

    if (new_pos > arena->commit_size)
        arena_commit_page_or_chunk(arena, new_pos);

    arena->pos = new_pos;
    void* result = arena->base + aligned_pos;

    // ways to get zeroed mem: 1. forced, 2. policy and not never
    if ( (zero == ArenaZero_Force) || ( (zero == ArenaZero_FollowPolicy) && (arena->flags & ArenaFlags_AlwaysZero) != 0 ))
        memset(result, 0, (size_t)size);

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

char* arena_push_cstring(Arena* arena, const char* src)
{
    size_t len = strlen(src);
    char *dst = (char*)arena_push(arena, (u64)len + 1, 1, ArenaZero_Force);
    memcpy(dst, src, len + 1);
    return dst;

}

static inline char* arena_push_cstring_fmtv(Arena* arena, const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    ASSERT (len >= 0);

    /* use 1-byte alignement to maximum packing */
    char* buffer = (char*)arena_push(arena, (u64)len + 1, 1, ArenaZero_Force);
    int written = vsnprintf(buffer, (size_t)len + 1, fmt, args);
    ASSERT(written == len);

    return buffer;
}

char* arena_push_cstring_fmt(Arena* arena, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* result = arena_push_cstring_fmtv(arena, fmt, args);
    va_end(args);
    return result;
}

str8  arena_push_str8_copy(Arena* arena, cstr8 src)
{
    str8 result;
    result.size = src.size;
    result.data = (u8*)arena_push(arena, src.size, 1, ArenaZero_FollowPolicy);
    memcpy(result.data, src.data, src.size);
    return result;

}

str8  arena_push_str8_from_cstring(Arena* arena, const char* src)
{
    size_t len = strlen(src);
    char*  dst = (char*)arena_push(arena, (u64)len + 1, 1, ArenaZero_Force);
    memcpy(dst, src, len + 1);

    str8 result;
    result.data = (u8*)dst;
    result.size = (u64)len; /* excludes the null terminator */
    return result;
}

static inline str8  arena_push_str8_fmtv(Arena* arena, const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args_copy);

    va_end(args_copy);

    ASSERT(len >= 0);

    char* buffer = (char*)arena_push(arena, (u64)len + 1, 1, ArenaZero_Force);

    int written = vsnprintf(buffer, (size_t)len + 1, fmt, args);
    ASSERT(written == len);

    str8 result;
    result.data = (u8*)buffer;
    result.size = (u64)len; /* excludes null terminator */
    return result;

}

str8  arena_push_str8_fmt(Arena* arena, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str8 result = arena_push_str8_fmtv(arena, fmt, args);
    va_end(args);
    return result;
}

str8  arena_read_file(Arena* arena, const char* path)
{
    str8 result = {0};
    void* h = os_file_open_for_read(path);
    if (!h) return result;

    u64 size = 0;
    if (!os_file_size(h, &size)) { os_file_close(h); return result; } /* recoverable: missing/inaccessible file */

    u64 mark = arena->pos;

    result.data = arena_push_array_nozero(arena, u8, size);
    result.size = size;

    b8 ok = os_file_read(h, result.data, size);
    os_file_close(h);

    if (!ok) { arena_pop_to(arena, mark); return (str8){0}; }

    return result;
}

str8  map_file(const char* path)
{
    void* h = os_file_open_for_read(path);
    if (!h) { return (str8){0}; }

    u64 size = 0;
    if (!os_file_size(h, &size)) { os_file_close(h); return (str8){0}; } /* recoverable: missing/inaccessible file */
    if (size == 0) { os_file_close(h); return (str8){.data=NULL, .size=0};}

    u8* data = (u8*)os_file_map(h, size);
    os_file_close(h);

    if(!data) { return (str8){0}; }

    return (str8){.data=data, .size=size};

}

void  unmap_file(str8 buf)
{
    os_file_unmap(buf.data, buf.size);
}

u64 time_mark(void)
{
    return os_time_now();
}

f64 time_elapsed_sec(u64 start, u64 end)
{
    return (f64)(end - start) / (f64)os_time_frequency();
}


#ifdef __cplusplus
}
#endif // __cplusplus

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
