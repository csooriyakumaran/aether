/*---------------------------------------------------------------------------*\
  AETHER

  Minimal Core Library for C/C++

  Author      : C. Sooriyakumaran
  Created     : 2026-06-18
  License     : MIT

  https://github.com/csooriyakumaran/aether

  DESCRIPTIONS
  ------------

  Core types, memory arenas, spans, assertions, and utility primitives.

  Do this:
      #define AETHER_IMPLEMENTATION
  before you include this file in *one* C or C++ file to create the implementation.

  // i.e.
  #include ...
  #include ...
  #include ...

  #define AETHER_IMPLEMENTATION
  #include "aether/aether.h"

  OPTIONAL OPT-OUT DEFINES
  ------------------------

  - AETHER_NO_MINMAX:       skips defining MIN/MAX if not already defined
  - AETHER_NO_ASSERT:       skips defining ASSERT if not already defined
  - AETHER_NO_ARRAY_COUNT:  skips defining ARRAY_COUNT if not already defined
  - AETHER_NO_NUMERIC_LIMITS: skips defining MIN/MAX for I8/I16/I32/I64/U8/U16/U32/U64

\*---------------------------------------------------------------------------*/
#ifndef AETHER_H_
#define AETHER_H_

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*-------- C O N F I G -------------------------------------------------------*/

#if defined(_MSC_VER)
    #define AETHER_DEBUG_BREAK() __debugbreak()
#else
    #define AETHER_DEBUG_BREAK() __builtin_trap()
#endif // _MSC_VER

#if defined(__cplusplus)
    #define ARENA_ALIGN(T) alignof(T)
#else
    #define ARENA_ALIGN(T) _Alignof(T)
#endif // __cplusplus

#if defined(__cplusplus)
    #define AETHER_LITERAL(T) T
#else
    #define AETHER_LITERAL(T) (T)
#endif // __cplusplus

#if defined(__cplusplus)
    #define AETHER_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define AETHER_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg) /* C11 */
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
    #define AETHER_ASSERT_(x) do {                                                  \
        if (!(x)) {                                                                 \
            fprintf(stderr, "ASSERT FAILED: %s [%s:%d]\n", #x, __FILE__, __LINE__); \
            AETHER_DEBUG_BREAK();                                                   \
        }                                                                           \
    } while (0)
#else
    #define AETHER_ASSERT_(x) ((void)0)
#endif // AETHER_ENABLE_ASSERTS

#ifndef AETHER_NO_ASSERT
    #ifndef ASSERT
    #define ASSERT(x) AETHER_ASSERT_(x)
    #endif // ASSERT
#endif // AETHER_NO_ASSERT

#define FATAL(msg) do {  \
    fprintf(stderr, "FATAL ERROR: %s [%s:%d]\n", msg, __FILE__, __LINE__); \
    AETHER_DEBUG_BREAK(); \
} while (0)

#define AETHER_I8_MIN_   (-0x7F - 1)
#define AETHER_I8_MAX_     0x7F
#define AETHER_U8_MAX_     0xFFu

#define AETHER_I16_MIN_  (-0x7FFF - 1)
#define AETHER_I16_MAX_    0x7FFF
#define AETHER_U16_MAX_    0xFFFFu

#define AETHER_I32_MIN_  (-0x7FFFFFFF - 1)
#define AETHER_I32_MAX_    0x7FFFFFFF
#define AETHER_U32_MAX_    0xFFFFFFFFu

#define AETHER_I64_MIN_  (-0x7FFFFFFFFFFFFFFFll - 1)
#define AETHER_I64_MAX_    0x7FFFFFFFFFFFFFFFll
#define AETHER_U64_MAX_    0xFFFFFFFFFFFFFFFFull

#ifndef AETHER_NO_NUMERIC_LIMITS
    #define I8_MIN AETHER_I8_MIN_
    #define I8_MAX AETHER_I8_MAX_
    #define U8_MAX AETHER_U8_MAX_

    #define I16_MIN AETHER_I16_MIN_
    #define I16_MAX AETHER_I16_MAX_
    #define U16_MAX AETHER_U16_MAX_

    #define I32_MIN AETHER_I32_MIN_
    #define I32_MAX AETHER_I32_MAX_
    #define U32_MAX AETHER_U32_MAX_

    #define I64_MIN AETHER_I64_MIN_
    #define I64_MAX AETHER_I64_MAX_
    #define U64_MAX AETHER_U64_MAX_
#endif // AETHER_NO_NUMERIC_LIMITS

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L )
    #ifndef true
    #define true 1
    #endif // true

    #ifndef false
    #define false 0
    #endif // false
#endif

/* ARRAYS ONLY: decays silently on pointers */
#define AETHER_ARRAY_COUNT_(a) (sizeof(a) / sizeof((a)[0]))

#ifndef AETHER_NO_ARRAY_COUNT
    #ifndef ARRAY_COUNT
    #define ARRAY_COUNT(a) AETHER_ARRAY_COUNT_(a)
    #endif // ARRAY_COUNT
#endif // AETHER_NO_ARRAY_COUNT

#define AETHER_MIN_(a, b) ((a) < (b) ? (a) : (b))
#define AETHER_MAX_(a, b) ((a) > (b) ? (a) : (b))

#ifndef AETHER_NO_MINMAX
    #ifndef MIN
    #define MIN(a, b) AETHER_MIN_(a, b)
    #endif // MIN
    #ifndef MAX
    #define MAX(a, b) AETHER_MAX_(a, b)
    #endif // MAX
#endif // AETHER_NO_MINMAX

/*-------- T Y P E S ---------------------------------------------------------*/

typedef signed char         i8; AETHER_STATIC_ASSERT(sizeof(i8)  == 1, "i8  != 1 byte");
typedef signed short       i16; AETHER_STATIC_ASSERT(sizeof(i16) == 2, "i16 != 2 bytes");
typedef signed int         i32; AETHER_STATIC_ASSERT(sizeof(i32) == 4, "i32 != 4 bytes");
typedef signed long long   i64; AETHER_STATIC_ASSERT(sizeof(i64) == 8, "i64 != 8 bytes");

typedef unsigned char       u8; AETHER_STATIC_ASSERT(sizeof(u8)  == 1, "u8  != 1 byte");
typedef unsigned short     u16; AETHER_STATIC_ASSERT(sizeof(u16) == 2, "u16 != 2 bytes");
typedef unsigned int       u32; AETHER_STATIC_ASSERT(sizeof(u32) == 4, "u32 != 4 bytes");
typedef unsigned long long u64; AETHER_STATIC_ASSERT(sizeof(u64) == 8, "u64 != 8 bytes");

typedef float              f32; AETHER_STATIC_ASSERT(sizeof(f32) == 4, "f32 != 4 bytes");
typedef double             f64; AETHER_STATIC_ASSERT(sizeof(f64) == 8, "f64 != 8 bytes");

typedef u8                  b8; AETHER_STATIC_ASSERT(sizeof(b8)  == 1, "b8  != 1 byte");
typedef u32                b32; AETHER_STATIC_ASSERT(sizeof(b32) == 4, "b32 != 4 bytes");

typedef struct bytes      {       u8* data; u64 size; } bytes;
typedef struct bytes_view { const u8* data; u64 size; } bytes_view;

/*-------- S T R I N G S -----------------------------------------------------*/

typedef bytes      str8;
typedef bytes_view str8_view;

static  inline bytes_view view_from_bytes(bytes b) { bytes_view v = {b.data, b.size}; return v; }
static  inline str8_view  view_from_str8(str8 s)   { str8_view  v = {s.data, s.size}; return v; }

#define STR(s) (AETHER_LITERAL(str8_view){ (const u8*)(s), sizeof(s) - 1 })
#define STR8_ARG(s) ((int)((s).size)), ((const char*)((s).data))
#define STR8_FMT "%.*s"

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

#define AETHER_ARENA_HEADER_SIZE 128

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
    u64 reserved_size;
    u64 commit_size;
    u64 pos;

    u32 granularity;
    ArenaFlags flags;
} Arena;

AETHER_STATIC_ASSERT(sizeof(Arena) <= AETHER_ARENA_HEADER_SIZE, "Arena header exceeds reserved header size");

typedef struct ArenaTemp {
    Arena* arena;
    u64 pos;
} ArenaTemp;

Arena* arena_alloc_ex(u64 reserve_size, u64 initial_commit_size, u32 commit_page_granularity, ArenaFlags flags);
Arena* arena_alloc(u64 reserve_size);
void   arena_release(Arena* arena);

void*  arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero);
void   arena_pop(Arena* arena, u64 amt);
void   arena_pop_to(Arena* arena, u64 pos);

void  arena_clear(Arena* arena);

static inline void* arena_push_array_(Arena* arena, u64 elem_size, u64 align, u64 count, ArenaZero zero)
{
    if (elem_size != 0 && count > AETHER_U64_MAX_ / elem_size) { AETHER_ASSERT_(!"arena_push array overflow"); return NULL; }
    return arena_push(arena, elem_size * count, align, zero);
}

// default arena_push will respect zeroing policy controlled by ArenaFlags
#define arena_push_t(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_ALIGN(T), ArenaZero_FollowPolicy)
#define arena_push_array(arena, T, n) (T*)arena_push_array_((arena), sizeof(T), ARENA_ALIGN(T), (n), ArenaZero_FollowPolicy)

// explicitly force zeroing after push regardless of ArenaFlags
#define arena_push_t_zero(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_ALIGN(T), ArenaZero_Force)
#define arena_push_array_zero(arena, T, n) (T*)arena_push_array_((arena), sizeof(T), ARENA_ALIGN(T), (n), ArenaZero_Force)

// explicitly force no-zeroing after push regardless of ArenaFlags
#define arena_push_t_nozero(arena, T) (T*)arena_push((arena), sizeof(T), ARENA_ALIGN(T), ArenaZero_Never)
#define arena_push_array_nozero(arena, T, n) (T*)arena_push_array_((arena), sizeof(T), ARENA_ALIGN(T), (n), ArenaZero_Never)

// strings on the arena
char* arena_push_cstring(Arena* arena, const char* src);
char* arena_push_cstring_fmt(Arena* arena, const char* fmt, ...);

str8  arena_push_str8_copy(Arena* arena, str8_view src);
str8  arena_push_str8_from_cstring(Arena* arena, const char* src);
str8  arena_push_str8_fmt(Arena* arena, const char* fmt, ...);

// temporary arenas
ArenaTemp arena_begin_temp(Arena* arena);
void      arena_end_temp(ArenaTemp temp);

/* -------- R I N G / C I R C U L A R - B U F F E R S ---------------------- */
typedef struct RingBuffer
{
    u8* base;
    u64 size;
    u64 read;
    u64 write;
} RingBuffer;

RingBuffer ring_buffer_alloc(u64 size);
void       ring_buffer_release(RingBuffer* rb);
u64        ring_buffer_available(RingBuffer* rb);
b8         ring_buffer_read(RingBuffer* rb, void* dst, u64 len);
b8         ring_buffer_write(RingBuffer* rb, const void* src, u64 len);
b8         ring_buffer_advance_read(RingBuffer* rb, u64 len);
bytes_view ring_buffer_peek(RingBuffer* rb, u64 len);

/* -------- S T R I N G - O P E R A T I O N S ------------------------------ */
str8_view view_from_c_str(const char* s);

// convert to null-terminated string
char*     c_str(Arena* arena, str8_view s);

b8        str8_eq(str8_view a, str8_view b);
i32       str8_cmp(str8_view a, str8_view b); /* memcmp-style ordering */
str8_view str8_slice(str8_view s, u64 start, u64 end);
str8_view str8_trim(str8_view s);
b8        str8_has_prefix(str8_view s, str8_view prefix);
b8        str8_cut(str8_view s, str8_view sep, str8_view* before, str8_view* after);

// todo(chris): additional string operations (find, split, etc.)
// str8_view* str8_split(Arena* arena str8_view s, str8_view delimiter, u64* out_len);
// b8         str*_find(str8_view s, str8_view sub);



/* ------- F I L E - I / O ------------------------------------------------- */

bytes arena_read_file(Arena* arena, const char* path);

// Read-only view into a memory mapped file
bytes_view map_file(const char* path);
void       unmap_file(bytes_view map);

/* ------- H E L P E R S --------------------------------------------------- */
u64 time_mark(void);
f64 time_elapsed_sec(u64 start, u64 end);

typedef struct HighResTimer
{
    u64 period_ticks;
    u64 next_deadline;
    u64 overrun;
    u64 spin_margin;
    void* os_timer;
} HighResTimer;

HighResTimer high_res_timer_create(f64 hz);
u64          high_res_timer_wait(HighResTimer* t);
void         high_res_timer_release(HighResTimer* t);

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

    /* required for ring buffers to avoid compile-time link requirement */
    typedef PVOID (WINAPI *VirtualAlloc2_fn)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, void*, ULONG);
    typedef PVOID (WINAPI *MapViewOfFile3_fn)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, void*, ULONG);
    typedef BOOL  (WINAPI *UnmapViewOfFile2_fn)(HANDLE, PVOID, ULONG);

    #ifndef MEM_RESERVE_PLACEHOLDER
    #define MEM_RESERVE_PLACEHOLDER 0x00040000
    #endif // MEM_RESERVE_PLACEHOLDER

    #ifndef MEM_REPLACE_PLACEHOLDER
    #define MEM_REPLACE_PLACEHOLDER 0x00004000
    #endif // MEM_REPLACE_PLACEHOLDER

    #ifndef MEM_PRESERVE_PLACEHOLDER
    #define MEM_PRESERVE_PLACEHOLDER 0x00000002
    #endif // MEM_PRESERVE_PLACEHOLDER

    #ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
    #endif // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION

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

static u64 os_mem_pagegranularity(void)
{
#ifdef _WIN32
    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    u64 page_granularity = (u64)sysinfo.dwAllocationGranularity;
    return page_granularity;
#else
    #error "AETHER: OS memory page granularity not implemented for this platform"
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

static b8 os_mem_release(void* ptr, u64 size)
{
#ifdef _WIN32
    (void)size;
    return VirtualFree(ptr, 0, MEM_RELEASE);
#else
    #error "AETHER: OS memory release not implemented for this platform"
#endif
}

/* todo(chris): maybe we need an os_file_open_ex,
 *              or os_file_open that allows user
 *              defined read/write mode */

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
    (void)path;
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
    if (!GetFileSizeEx(handle, &filesize))
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

    while (remaining > 0)
    {
        /* Note(Chris):
         * - capping at MAXDWORD (~4 GB) may be excessive
         * - could reduce in future to 1 GB */
        DWORD chunk = (remaining > MAXDWORD) ? MAXDWORD : (DWORD)remaining;
        DWORD bytes_read = 0;

        if (!ReadFile(handle, cursor, chunk, &bytes_read, NULL))
            return false;

        if (bytes_read == 0) /* EOF before reaching requested size */
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
    #error "AETHER: OS file map not implemented for this platform"
#endif
}

static void* os_mem_reserve_ring(u64 size)
{
#ifdef _WIN32
    VirtualAlloc2_fn va2 = (VirtualAlloc2_fn)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "VirtualAlloc2");
    if (!va2) return NULL;
    return va2(NULL, NULL, 2*size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
#else
    #error "AETHER: OS memory ring allocation not implemented for this platform"
#endif
}

static b8 os_mem_split_ring(void* ptr, u64 size)
{
#ifdef _WIN32
    return VirtualFree(ptr, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) != 0;
#else
    #error "AETHER: OS memory ring splitting not implemented for this platform"
#endif
}

static void* os_mem_map_ring(void* ptr, u64 size)
{
#ifdef _WIN32
    MapViewOfFile3_fn mvf3 = (MapViewOfFile3_fn)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "MapViewOfFile3");
    if (!mvf3) return NULL;

    DWORD  hi   = (DWORD)(size >> 32);
    DWORD  lo   = (DWORD)(size & 0xFFFFFFFF);
    HANDLE hmap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi, lo, NULL);
    if (!hmap) return NULL;

    PVOID base   = mvf3(hmap, NULL, (u8*)ptr,        0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    PVOID mirror = mvf3(hmap, NULL, (u8*)ptr + size, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    CloseHandle(hmap);

    if (!base || !mirror) return NULL;
    return (void*)base;
#else
    #error "AETHER: OS mem map ring not implemented for this platform"
#endif
}

static b8 os_mem_release_ring(void* ptr, u64 size)
{
#ifdef _WIN32
    UnmapViewOfFile2_fn umvf2 = (UnmapViewOfFile2_fn)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "UnmapViewOfFile2");
    if (!umvf2) return false;

    umvf2(GetCurrentProcess(), ptr,           MEM_PRESERVE_PLACEHOLDER);
    umvf2(GetCurrentProcess(), (u8*)ptr+size, MEM_PRESERVE_PLACEHOLDER);

    BOOL base_freed   = VirtualFree(     ptr,        0, MEM_RELEASE);
    BOOL mirror_freed = VirtualFree((u8*)ptr + size, 0, MEM_RELEASE);
    return base_freed && mirror_freed;
#else
    #error "AETHER: OS memory release not implemented for this platform"
#endif
}

static void os_file_unmap(const void* ptr, u64 size)
{
#ifdef _WIN32
    (void)size;
    UnmapViewOfFile(ptr);
#else
    #error "AETHER: OS file unmap not implemented for this platform"
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

static void* os_create_timer(void)
{
#ifdef _WIN32
    HANDLE h = CreateWaitableTimerExW(
        NULL,
        NULL,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS
    );
    if (!h) return NULL;
    return (void*)h;
#else
    #error "AETHER: OS timer not implemented for this platform"
#endif
}

static void os_timer_sleep(void* h, u64 sleep_ticks)
{
#ifdef _WIN32

    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)(sleep_ticks * 10000000ull / os_time_frequency());
    if (!SetWaitableTimer((HANDLE)h, &due, 0, NULL, NULL, FALSE)) return;
    WaitForSingleObject(h, INFINITE);
    return;
#else
    #error "AETHER: OS set timer not implemented for this platform"
#endif
}

static void os_timer_release(void* h)
{
#ifdef _WIN32
    CloseHandle(h);
#else
    #error "AETHER: OS release timer not implemented for this platform"
#endif
}

static void os_cpu_relax(void)
{
#ifdef _WIN32
    YieldProcessor();
#else
    #error "AETHER: OS cpu relax not implemented for this platform"
#endif
}

static u64 round_up_power_2(u64 n)
{
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

static u64 align_forward_u64(u64 value, u64 align)
{
    AETHER_ASSERT_(align != 0);
    AETHER_ASSERT_((align & (align - 1)) == 0);

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

    new_commit_size = AETHER_MIN_(new_commit_size, arena->reserved_size);

    b8 committed = os_mem_commit((u8*)arena + arena->commit_size, new_commit_size - arena->commit_size);
    if (!committed) FATAL("Memory commit failed");

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
        os_mem_decommit((u8*)arena + new_commit_size, decommit_size);
        arena->commit_size = new_commit_size;
    }

    return;
}

Arena* arena_alloc_ex(u64 reserve_size, u64 initial_commit_size, u32 commit_page_granularity, ArenaFlags flags)
{
    AETHER_ASSERT_(reserve_size > 0);

    u64 pagesize = os_mem_pagesize();

    reserve_size = align_forward_u64(reserve_size, pagesize);
    initial_commit_size  = align_forward_u64(initial_commit_size, pagesize);

    if (initial_commit_size > reserve_size)
        initial_commit_size = reserve_size;

    if (initial_commit_size < pagesize)
        initial_commit_size = pagesize;

    u8* base = (u8*)os_mem_reserve(reserve_size);
    if (!base) FATAL("Failed to reserve memory");
    if (!os_mem_commit(base, initial_commit_size)) FATAL("Failed to commit memory");

    Arena* arena         = (Arena*)base;
    arena->reserved_size = reserve_size;
    arena->commit_size   = initial_commit_size;
    arena->pos           = AETHER_ARENA_HEADER_SIZE;
    arena->flags         = flags;
    arena->granularity   = commit_page_granularity;

    return arena;

}

Arena* arena_alloc(u64 reserve_size)
{
#if AETHER_ENABLE_ASSERTS
    // Debug defaults
    u64        initial_commit = AETHER_ARENA_HEADER_SIZE;
    ArenaFlags flags          = ArenaFlags_AlwaysZero |
                                ArenaFlags_DebugFillOnClear |
                                ArenaFlags_Decommit;
    u32 granularity           = 1; /* commit and decommit single-page units */
#else
    // Performance defaults
    u64        initial_commit = AETHER_ARENA_HEADER_SIZE;
    ArenaFlags flags          = ArenaFlags_None;
    u32        granularity    = 1;
#endif
    return arena_alloc_ex(reserve_size, initial_commit, granularity, flags);
}

void arena_release(Arena* arena)
{
    if (!arena) return;
    os_mem_release(arena, arena->reserved_size);
}

void* arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero)
{
    AETHER_ASSERT_(arena->pos <= arena->reserved_size);

    u64 aligned_pos = align_forward_u64(arena->pos, align);
    AETHER_ASSERT_(aligned_pos <= arena->reserved_size);

    // catch u64 overflow
    if (size > arena->reserved_size - aligned_pos)
    {
        AETHER_ASSERT_(!"arena_push overflow: allocation exceeds reserved size");
        return NULL;
    }

    u64 new_pos = aligned_pos + size;
    AETHER_ASSERT_(new_pos <= arena->reserved_size);

    if (new_pos > arena->commit_size)
        arena_commit_page_or_chunk(arena, new_pos);

    arena->pos = new_pos;
    void* result = (u8*)arena + aligned_pos;

    // ways to get zeroed mem: 1. forced, 2. policy and not never
    if ( (zero == ArenaZero_Force) || ( (zero == ArenaZero_FollowPolicy) && (arena->flags & ArenaFlags_AlwaysZero) != 0 ))
        memset(result, 0, (size_t)size);

    return result;
}

void arena_pop_to(Arena* arena, u64 pos)
{
    if (pos < AETHER_ARENA_HEADER_SIZE) pos = AETHER_ARENA_HEADER_SIZE;
    if (pos > arena->pos)               pos = arena->pos;

    if ((arena->flags & ArenaFlags_DebugFillOnClear) != 0)
        memset((u8*)arena + pos, 0xDD, arena->pos - pos);

    if ((arena->flags & ArenaFlags_Decommit) != 0)
        arena_decommit_tail(arena, pos);

    arena->pos = pos;
}


void arena_pop(Arena* arena, u64 amt)
{
    if (amt > arena->pos)
        amt = arena->pos;

    arena_pop_to(arena, arena->pos - amt);
}

void arena_clear(Arena* arena)
{
    arena_pop_to(arena, AETHER_ARENA_HEADER_SIZE);
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

    AETHER_ASSERT_(len >= 0);

    /* use 1-byte alignment to maximum packing */
    char* buffer = (char*)arena_push(arena, (u64)len + 1, 1, ArenaZero_Force);
    int written = vsnprintf(buffer, (size_t)len + 1, fmt, args);
    AETHER_ASSERT_(written == len);

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

str8  arena_push_str8_copy(Arena* arena, str8_view src)
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

    AETHER_ASSERT_(len >= 0);

    char* buffer = (char*)arena_push(arena, (u64)len + 1, 1, ArenaZero_Force);

    int written = vsnprintf(buffer, (size_t)len + 1, fmt, args);
    AETHER_ASSERT_(written == len);

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

ArenaTemp arena_begin_temp(Arena* arena)
{
    ArenaTemp tmp;
    tmp.arena = arena;
    tmp.pos   = arena->pos;
    return tmp;
}

void arena_end_temp(ArenaTemp temp)
{
    AETHER_ASSERT_(temp.pos <= temp.arena->pos);
    arena_pop_to(temp.arena, temp.pos);
}

RingBuffer ring_buffer_alloc(u64 size)
{
    AETHER_ASSERT_(size > 0);

    RingBuffer rb = {0};

    u64 size_pow2        = round_up_power_2(size);
    u64 page_granularity = os_mem_pagegranularity(); /* should already be power of 2 */
    u64 ring_size        = AETHER_MAX_(size_pow2, page_granularity);

    rb.base = (u8*)os_mem_reserve_ring(ring_size);
    if (!rb.base) FATAL("Failed to allocated RingBuffer\n");

    if (!os_mem_split_ring(rb.base, ring_size))
    {
        os_mem_release(rb.base, ring_size);
        FATAL("Failed to split RingBuffer\n");

    }

    u8* base = (u8*)os_mem_map_ring(rb.base, ring_size);
    if (!base || rb.base != base)
    {
        os_mem_release_ring(rb.base, ring_size);
        FATAL("Failed to double-map RingBuffer\n");
    }

    rb.size  = ring_size;
    rb.read  = 0;
    rb.write = 0;

    return rb;
}

u64 ring_buffer_available(RingBuffer* rb)
{
    if (!rb || !rb->base) return 0;
    return rb->write - rb->read;
}

void ring_buffer_release(RingBuffer* rb)
{
    if (!rb || !rb->base) return;
    os_mem_release_ring(rb->base, rb->size);
    rb->base  = NULL;
    rb->size  = 0;
    rb->read  = 0;
    rb->write = 0;
}

b8 ring_buffer_read(RingBuffer* rb, void* dst, u64 len)
{

    bytes_view view = ring_buffer_peek(rb, len);
    if (!view.size) return false;

    memcpy(dst, view.data, view.size);
    ring_buffer_advance_read(rb, view.size);

    return true;
}

b8 ring_buffer_write(RingBuffer* rb, const void* src, u64 len)
{
    if (!rb || !rb->base) return false;
    if (len > rb->size ) return false;
    /* reject if it will overwrite unread data */
    if (len > rb->size - (rb->write - rb->read)) return false;

    u64 write_idx = rb->write & (rb->size - 1);
    memcpy(rb->base + write_idx, src, len);
    rb->write += len;
    return true;
}

b8  ring_buffer_advance_read(RingBuffer* rb, u64 len)
{
    if (!rb || !rb->base) return false;
    if (len > rb->write - rb->read) return false;
    rb->read += len;
    return true;
}

bytes_view ring_buffer_peek(RingBuffer* rb, u64 len)
{
    bytes_view v = {0};
    if (!rb || !rb->base) return v;

    u64 available = rb->write - rb->read;

    if (len <= available)
    {
        u64 read_idx = rb->read & (rb->size - 1);
        v.data = rb->base + read_idx;
        v.size = len;
        return v;
    }
    return v;
}

str8_view view_from_c_str(const char* s)
{
    str8_view v = {0};

    if (!s) return v;

    u64 len = (u64)strlen(s);
    if (!len) return v;

    v.data = (const u8*)s;
    v.size = len;

    return v;
}

char* c_str(Arena* arena, str8_view s)
{
    AETHER_ASSERT_(arena != NULL);
    AETHER_ASSERT_(s.data != NULL || s.size == 0); /* NULL data with size > 0 is a caller bug */

    char* out = (char*)arena_push(arena, s.size + 1, 1, ArenaZero_Never);
    if (s.size) memcpy(out, s.data, s.size);
    out[s.size] = '\0';
    return out;
}

bytes  arena_read_file(Arena* arena, const char* path)
{
    bytes result = {0};
    void* h = os_file_open_for_read(path);
    if (!h) return result;

    u64 size = 0;
    if (!os_file_size(h, &size)) { os_file_close(h); return result; } /* recoverable: missing/inaccessible file */

    u64 mark = arena->pos;

    result.data = arena_push_array_nozero(arena, u8, size);
    result.size = size;

    b8 ok = os_file_read(h, result.data, size);
    os_file_close(h);

    if (!ok) {
        arena_pop_to(arena, mark);
        result.data = NULL;
        result.size = 0;
        return result;
    }

    return result;
}

bytes_view  map_file(const char* path)
{
    bytes_view v = {0};
    void* h = os_file_open_for_read(path);
    if (!h) { return v; }

    u64 size = 0;
    if (!os_file_size(h, &size)) { os_file_close(h); return v; } /* recoverable: missing/inaccessible file */
    if (size == 0) { os_file_close(h); return v; }

    u8* data = (u8*)os_file_map(h, size);
    os_file_close(h);

    if (!data) { return v; }

    v.data = data;
    v.size = size;
    return v;
}

void  unmap_file(bytes_view buf)
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

b8 str8_eq(str8_view a, str8_view b)
{
    if (a.size != b.size) return false;
    if (a.data == b.data) return true;
    return memcmp(a.data, b.data, a.size) == 0;
}

i32 str8_cmp(str8_view a, str8_view b)
{
    u64 n = AETHER_MIN_(a.size, b.size);
    i32 r = memcmp(a.data, b.data, n);
    if (r != 0) return r;
    if (a.size != b.size) return (a.size < b.size) ? -1 : 1;
    return 0;
}

b8 str8_cut(str8_view s, str8_view sep, str8_view* before, str8_view* after)
{
    AETHER_ASSERT_(s.data   != NULL || s.size   == 0);
    AETHER_ASSERT_(sep.data != NULL || sep.size == 0);

    str8_view empty = {0};

    if (sep.size == 0 || sep.size > s.size)
    {
        if (before) *before = s;
        if (after)  *after  = empty;
        return false;
    }

    for (u64 i = 0; i + sep.size <= s.size; ++i)
    {
        if (memcmp(s.data + i, sep.data, sep.size) == 0)
        {
            if (before) *before = str8_slice(s, 0, i);
            if (after)  *after  = str8_slice(s, i + sep.size, s.size);
            return true;
        }
    }

    if (before) *before = s;
    if (after)  *after  = empty;
    return false;
}

b8 str8_has_prefix(str8_view s, str8_view prefix)
{
    if (!prefix.data || !prefix.size || !s.data || s.size == 0) return false;

    if (s.size < prefix.size) return false;

    for (u64 i = 0; i < prefix.size; ++i)
        if (s.data[i] != prefix.data[i]) return false;

    return true;
}

str8_view str8_slice(str8_view s, u64 start, u64 end)
{
    AETHER_ASSERT_(start <= end && end <= s.size);
    str8_view v;
    v.data = s.data + start;
    v.size = end - start;
    return v;
}

static b8 char_is_ws(u8 c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

str8_view str8_trim(str8_view s)
{
    u64 start = 0;
    while (start < s.size && char_is_ws(s.data[start])) start++;

    u64 end   = s.size;
    while (end > start && char_is_ws(s.data[end-1])) end--;

    return str8_slice(s, start, end);
}

HighResTimer high_res_timer_create(f64 hz)
{
    AETHER_ASSERT_(hz > 0);

    HighResTimer t = {0};

    u64 freq         = os_time_frequency();
    u64 period_ticks = (u64)((f64)freq / hz + 0.5);
    u64 now          = os_time_now();
    void* os_timer   = os_create_timer();

    if (!os_timer)
    {
        FATAL("Failed to create platform timer");
        return t;
    }

    t.period_ticks  = period_ticks;
    t.os_timer      = os_timer;
    t.next_deadline = now;
    t.spin_margin   = freq / 1000;

    return t;
}

u64 high_res_timer_wait(HighResTimer* t)
{

    if (!t->os_timer) return 0;

    u64 now = os_time_now();
    t->next_deadline += t->period_ticks;

    if (t->next_deadline <= now)
    {
        u64 misses = 1 + (now - t->next_deadline) / t->period_ticks;
        t->overrun += misses;
        t->next_deadline = now;
        return misses;
    }

    if (t-> next_deadline > now + t->spin_margin)
        os_timer_sleep(t->os_timer, t->next_deadline - now - t->spin_margin);

    while (os_time_now() < t->next_deadline)
        os_cpu_relax();

    return 0;
}

void high_res_timer_release(HighResTimer* t)
{
    if (!t || !t->os_timer) return;
    os_timer_release(t->os_timer);
    t->os_timer      = NULL;
    t->period_ticks  = 0;
    t->next_deadline = 0;
    t->overrun       = 0;
    return;
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // AETHER_IMPLEMENTATION
/*---------------------------------------------------------------------------*/

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
