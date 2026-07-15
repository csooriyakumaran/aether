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

  LINKAGE DEFINES
  --------------

  - AETHER_STATIC             API functions become static (private to the TU)
                              Requires AETHER_IMPLEMENTATION in the *same* file;
                              no other TU can use the library.
  - AETHER_BUILD_DLL          building aether as a shared library. Define together
                              with AETHER_IMPLEMENTATION in the DLL's TU; marks the
                              API dllexport (visibility("default") on POSIX)
  - AETHER_DLL                consuming aether as a shared library; marks the API
                              dllimport. Do not define AETHER_IMPLEMENTATION.

  CONFIG DEFINES
  --------------

  - AETHER_BUILD_DEBUG=0|1    force debug/release behaviour
                              (default: 1 unless NDEBUG is defined)
  - AETHER_ENABLE_ASSERTS=0|1 force asserts on/off
                              (default: AETHER_BUILD_DEBUG)

  OPTIONAL OPT-OUT DEFINES
  ------------------------

  - AETHER_NO_MINMAX:          skips defining MIN/MAX if not already defined
  - AETHER_NO_ASSERT:          skips defining ASSERT if not already defined
  - AETHER_NO_ARRAY_COUNT:     skips defining ARRAY_COUNT if not already defined
  - AETHER_NO_NUMERIC_LIMITS:  skips defining *_MIN / *_MAX-style limits
                               e.g., for I8/I16/I32/I64/U8/U16/U32/U64

\*---------------------------------------------------------------------------*/
#ifndef AETHER_H_
#define AETHER_H_

/*-------- C O N T E X T ----------------------------------------------------*/

// COMPILER
#if defined(__clang__)
    #define AETHER_COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define AETHER_COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define AETHER_COMPILER_GCC 1
#endif 

#if !defined(AETHER_COMPILER_MSVC)
    #define AETHER_COMPILER_MSVC 0
#endif
#if !defined(AETHER_COMPILER_GCC)
    #define AETHER_COMPILER_GCC 0
#endif
#if !defined(AETHER_COMPILER_CLANG)
    #define AETHER_COMPILER_CLANG 0
#endif

#if !(AETHER_COMPILER_MSVC || AETHER_COMPILER_CLANG || AETHER_COMPILER_GCC)
    #error "AETHER: unsupported compiler"
#endif

// OS
#if defined(_WIN32)
    #define AETHER_OS_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define AETHER_OS_MAC 1
    #endif
#elif defined(__linux__)
    #define AETHER_OS_LINUX 1
    #if defined(__ANDROID__)
        #define AETHER_OS_ANDROID 1
    #endif
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define AETHER_OS_BSD 1
#endif

#if !defined(AETHER_OS_WINDOWS)
    #define AETHER_OS_WINDOWS 0
#endif
#if !defined(AETHER_OS_MAC)
    #define AETHER_OS_MAC 0
#endif
#if !defined(AETHER_OS_LINUX)
    #define AETHER_OS_LINUX 0
#endif
#if !defined(AETHER_OS_ANDROID)
    #define AETHER_OS_ANDROID 0
#endif
#if !defined(AETHER_OS_BSD)
    #define AETHER_OS_BSD 0
#endif

#define AETHER_OS_POSIX (AETHER_OS_MAC || AETHER_OS_LINUX || AETHER_OS_BSD)

#if !(AETHER_OS_WINDOWS || AETHER_OS_LINUX || AETHER_OS_MAC || AETHER_OS_BSD)
    #error "AETHER: unsupported os"
#endif

// ARCH
#if AETHER_COMPILER_MSVC
    #if defined(_M_X64)
        #define AETHER_ARCH_X64 1
    #elif defined (_M_ARM64)
        #define AETHER_ARCH_ARM64 1
    #elif defined (_M_IX86)
        #define AETHER_ARCH_X86 1
    #endif
#elif AETHER_COMPILER_CLANG || AETHER_COMPILER_GCC
    #if defined(__x86_64__)
        #define AETHER_ARCH_X64 1
    #elif defined(__aarch64__)
        #define AETHER_ARCH_ARM64 1
    #elif defined(__i386__)
        #define AETHER_ARCH_X86 1
    #endif
#endif

#if !defined(AETHER_ARCH_X64)
    #define AETHER_ARCH_X64 0
#endif
#if !defined(AETHER_ARCH_ARM64)
    #define AETHER_ARCH_ARM64 0
#endif
#if !defined(AETHER_ARCH_X86)
    #define AETHER_ARCH_X86 0
#endif 

#if !(AETHER_ARCH_X64 || AETHER_ARCH_ARM64 || AETHER_ARCH_X86)
    #error "AETHER: unsupported architecture"
#endif

// LANG
#if defined(__cplusplus)
    #define AETHER_LANG_CPP 1
    #define AETHER_LANG_C 0
#else
    #define AETHER_LANG_C 1
    #define AETHER_LANG_CPP 0
#endif

#if AETHER_LANG_C && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define AETHER_LANG_C23 1
#else
    #define AETHER_LANG_C23 0
#endif

// BUILD
#ifndef AETHER_BUILD_DEBUG
    #if !defined(NDEBUG)
        #define AETHER_BUILD_DEBUG 1
    #else
        #define AETHER_BUILD_DEBUG 0
    #endif
#endif // AETHER_BUILD_DEBUG

#if defined(AETHER_STATIC) && defined(AETHER_DLL)
#error "AETHER_STATIC and AETHER_DLL are mutually exclusive"
#endif

#if defined(AETHER_STATIC) && defined(AETHER_BUILD_DLL)
    #error "AETHER_STATIC and AETHER_BUILD_DLL are mutually exclusive"
#endif

#if defined(AETHER_IMPLEMENTATION) && defined(AETHER_DLL) && !defined(AETHER_BUILD_DLL)
    #error "Cannot compile the implementation in DLL-import mode; define AETHER_BUILD_DLL"
#endif

#if AETHER_OS_WINDOWS
    #define AETHER_DLL_EXPORT __declspec(dllexport)
    #define AETHER_DLL_IMPORT __declspec(dllimport)
#else
    #define AETHER_DLL_EXPORT __attribute__((visibility("default")))
    #define AETHER_DLL_IMPORT extern
#endif

#if defined(AETHER_BUILD_DLL)
    #define AETHER_API AETHER_DLL_EXPORT
#elif defined(AETHER_DLL)
    #define AETHER_API AETHER_DLL_IMPORT
#elif defined(AETHER_STATIC)
    #define AETHER_API static
#else
    #define AETHER_API extern
#endif


/*---------------------------------------------------------------------------*/

#if !AETHER_OS_WINDOWS
    #error "AETHER: Currently only supports Windows"
#endif 

#if !(AETHER_ARCH_X64 || AETHER_ARCH_ARM64)
    #error "AETHER: unsupported architecture (64-bit x64/arm64 required)"
#endif

/*---------------------------------------------------------------------------*/

#define internal      static
#define global        static
#define local_persist static

#include <stdio.h>
#include <stddef.h>

#if AETHER_COMPILER_MSVC
    #include <intrin.h> /* for atomics */
#endif // AETHER_COMPILER_MSVC

#if AETHER_LANG_CPP
extern "C"
{
#endif // AETHER_LANG_CPP

/*-------- C O N F I G -------------------------------------------------------*/

#if AETHER_COMPILER_MSVC
    #define AETHER_DEBUG_BREAK() __debugbreak()
#else
    #define AETHER_DEBUG_BREAK() __builtin_trap()
#endif // AETHER_COMPILER_MSVC

#if AETHER_LANG_CPP
    #define ARENA_ALIGN(T) alignof(T)
#else
    #define ARENA_ALIGN(T) _Alignof(T)
#endif // AETHER_LANG_CPP

#if AETHER_LANG_CPP
    #define AETHER_LITERAL(T) T
#else
    #define AETHER_LITERAL(T) (T)
#endif // AETHER_LANG_CPP

#if AETHER_LANG_CPP
    #define AETHER_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define AETHER_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg) /* C11 */
#endif // AETHER_LANG_CPP

/*----------------------------------------------------------------------------*/

#ifndef AETHER_ENABLE_ASSERTS
    #if AETHER_BUILD_DEBUG
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

#if AETHER_LANG_C && !AETHER_LANG_C23
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

static  inline bytes_view view_from_bytes(bytes b) { bytes_view v = {b.data, b.size}; return v; }
static  inline bytes_view view_from_raw(const void* data, u64 size) { bytes_view v = {(const u8*)data, size}; return v; }

/*-------- A T O M I C S  ----------------------------------------------------*/

AETHER_STATIC_ASSERT(sizeof(void*) == 8, "aether atomics require a 64-bit target");

#if AETHER_COMPILER_GCC || AETHER_COMPILER_CLANG
    #define AETHER_ATOMICS_GNU  1
    #define AETHER_ATOMICS_MSVC 0
#elif AETHER_COMPILER_MSVC && (AETHER_ARCH_X64 || AETHER_ARCH_ARM64)
    #define AETHER_ATOMICS_GNU  0
    #define AETHER_ATOMICS_MSVC 1
#else
    #error "AETHER atomics: unsupported compiler/architecture pair (GCC/Clang on any arch, or MSVC on x64/arm64)"
#endif

#if AETHER_ATOMICS_MSVC
    #if AETHER_ARCH_X64
        #define AETHER_MSVC_BARRIER_() _ReadWriteBarrier()
    #else
        #define AETHER_MSVC_BARRIER_() __dmb(0x0B)
    #endif
#endif

static inline u64 atomic_load_acq_u64(const u64* p)
{
#if AETHER_ATOMICS_GNU
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
#else
    u64 v = (u64)__iso_volatile_load64((const volatile __int64*)p);
    AETHER_MSVC_BARRIER_();
    return v;
#endif 
}

static inline void atomic_store_rel_u64(u64* p, u64 v)
{
#if AETHER_ATOMICS_GNU
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
#else
    AETHER_MSVC_BARRIER_();
    __iso_volatile_store64((volatile __int64*)p, (__int64)v);
#endif 
}

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

AETHER_API Arena* arena_alloc_ex(u64 reserve_size, u64 initial_commit_size, u32 commit_page_granularity, ArenaFlags flags);
AETHER_API Arena* arena_alloc(u64 reserve_size);
AETHER_API void   arena_release(Arena* arena);
AETHER_API void*  arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero);
AETHER_API void   arena_pop_to(Arena* arena, u64 pos);
AETHER_API void   arena_pop(Arena* arena, u64 amt);
AETHER_API void   arena_clear(Arena* arena);

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

// temporary arenas

typedef struct ArenaTemp {
    Arena* arena;
    u64 pos;
} ArenaTemp;

AETHER_API ArenaTemp arena_begin_temp(Arena* arena);
AETHER_API void      arena_end_temp(ArenaTemp temp);

/* -------- R I N G / C I R C U L A R - B U F F E R S ---------------------- */

/* - Ring buffers are safe for exactly one producer thread and one consumer thread
 *   more of either requires external synchronization
 * - peeked views become invalid after the matching advance_read */

typedef struct RingBuffer
{
    u8* base;
    u64 size;
    u64 read;
    u64 write;
} RingBuffer;

AETHER_API RingBuffer ring_buffer_alloc(u64 size);
AETHER_API void       ring_buffer_release(RingBuffer* rb);
AETHER_API u64        ring_buffer_available(RingBuffer* rb);
AETHER_API b8         ring_buffer_read(RingBuffer* rb, void* dst, u64 len);
AETHER_API b8         ring_buffer_write(RingBuffer* rb, const void* src, u64 len);
AETHER_API b8         ring_buffer_advance_read(RingBuffer* rb, u64 len);
AETHER_API bytes_view ring_buffer_peek(RingBuffer* rb, u64 len);

/*-------- S T R I N G S -----------------------------------------------------*/

/* note(chris):
 *    - str8's pushed onto an arena will be nul-terminated by convention
 *    - nul-termination is not included in size
 *    - str8_view has no guarantee of nul-termination
 */

typedef bytes      str8;
typedef bytes_view str8_view;

typedef struct str16      {       u16* data; u64  size; } str16;
typedef struct str16_view { const u16* data; u64  size; } str16_view;

static  inline str8_view  view_from_str8(str8 s)   { str8_view   v = {s.data, s.size}; return v; }
static  inline str16_view view_from_str16(str16 s) { str16_view  v = {s.data, s.size}; return v; }

#define STR(s) (AETHER_LITERAL(str8_view){ (const u8*)(s), sizeof(s) - 1 }) /*STRING LITERALS ONLY: decays silently on pointers */

#define STR8_ARG(s) ((int)((s).size)), ((const char*)((s).data))
#define STR8_FMT "%.*s"

typedef struct Str8Node Str8Node;
struct Str8Node
{
    Str8Node* next;
    str8_view  v;
};

typedef struct Str8List
{
    Str8Node* first;
    Str8Node* last;
    u64        count;
    u64        total_len;
} Str8List;

typedef struct Str8Array
{
    str8_view* items;
    u64        count;
} Str8Array;

typedef u8 Str8SplitFlags;
enum Str8SplitFlags_
{
    Str8SplitFlags_None      = 0u,
    Str8SplitFlags_SkipEmpty = BIT8(0),
};

// --- construction --- 
AETHER_API char*     c_str(Arena* arena, str8_view s);
AETHER_API char*     c_str_push_copy(Arena* arena, const char* src);
AETHER_API char*     c_str_push_fmt(Arena* arena, const char* fmt, ...);

AETHER_API str8      str8_push_copy(Arena* arena, str8_view src);
AETHER_API str8      str8_push_c_str(Arena* arena, const char* src);
AETHER_API str8      str8_push_fmt(Arena* arena, const char* fmt, ...);
AETHER_API str8      str8_concat(Arena* arena, str8_view a, str8_view b);

// --- view / slices --- (no allocation)
AETHER_API str8_view view_from_c_str(const char* s);
AETHER_API str8_view str8_slice(str8_view s, u64 start, u64 end);  /* return substr from [start, end) */
AETHER_API str8_view str8_skip(str8_view s, u64 n);                /* skip first n characters */
AETHER_API str8_view str8_drop(str8_view s, u64 n);                /* drop last n characters */
AETHER_API str8_view str8_trim(str8_view s);                       /* trim whitespace from both ends */
AETHER_API str8_view str8_trim_left(str8_view s);                  /* trim whitespace from left */
AETHER_API str8_view str8_trim_right(str8_view s);                 /* trim whitespace from right */

// --- queries ---       (no allocation)
AETHER_API b8        str8_eq(str8_view a, str8_view b);
AETHER_API b8        str8_eq_nocase(str8_view a, str8_view b);
AETHER_API b8        str8_has_prefix(str8_view s, str8_view prefix);
AETHER_API b8        str8_has_suffix(str8_view s, str8_view suffix);
AETHER_API b8        str8_find(str8_view s, str8_view needle, u64* pos);
AETHER_API b8        str8_find_last(str8_view s, str8_view needle, u64* pos);
AETHER_API b8        str8_find_char(str8_view s, u8 c, u64* pos);
AETHER_API i32       str8_cmp(str8_view a, str8_view b); /* memcmp-style ordering */

// --- cut / split / list / join --- 
AETHER_API b8        str8_cut(str8_view s, str8_view sep, str8_view* before, str8_view* after);
AETHER_API Str8List  str8_split(Arena* arena, str8_view s, str8_view sep, Str8SplitFlags flags);
AETHER_API void      str8_list_push(Arena* arena, Str8List* list, str8_view v);
AETHER_API void      str8_list_push_fmt(Arena* arena, Str8List* list, const char* fmt, ...);
AETHER_API str8      str8_join(Arena* arena, Str8List* list, str8_view sep);
AETHER_API Str8Array str8_list_to_array(Arena* arena, Str8List* list);

// --- transforms ---     (allocation)
AETHER_API str8      str8_to_upper(Arena* arena, str8_view s);
AETHER_API str8      str8_to_lower(Arena* arena, str8_view s);
AETHER_API str8      str8_replace(Arena* arena, str8_view s, str8_view old, str8_view target); /* split + join*/

// --- parsing --- 
AETHER_API b8        str8_to_u64(str8_view s, u64* out);
AETHER_API b8        str8_to_i64(str8_view s, i64* out);
AETHER_API b8        str8_to_f64(str8_view s, f64* out); /* limits to 64 character */

// --- paths --- 
// todo(chris): do this


/* ------- F I L E - I / O ------------------------------------------------- */

AETHER_API bytes file_read(Arena* arena, const char* path);
AETHER_API u64   file_write(const char* path, bytes_view data);

// Read-only view into a memory mapped file
AETHER_API bytes_view file_map(const char* path);
AETHER_API void       file_unmap(bytes_view map);

/* ------- T I M E R S ----------------------------------------------------- */
AETHER_API u64 time_mark(void);
AETHER_API f64 time_elapsed_sec(u64 start, u64 end);

typedef struct HighResTimer
{
    u64 period_ticks;
    u64 next_deadline;
    u64 overrun;
    u64 spin_margin;
    void* os_timer;
} HighResTimer;

AETHER_API HighResTimer high_res_timer_create(f64 hz);
AETHER_API u64          high_res_timer_wait(HighResTimer* t);
AETHER_API void         high_res_timer_release(HighResTimer* t);

/* ------- T H R E A D S  -------------------------------------------------- */

typedef int (*thread_fn)(void* user);
typedef struct Thread { void* handle; } Thread;

typedef u8 ThreadPriority;

enum ThreadPriority_
{
    ThreadPriority_Normal = 0,
    ThreadPriority_High,
    ThreadPriority_TimeCritical,
};

AETHER_API Thread thread_create(thread_fn fn, void* user);
AETHER_API b8     thread_join(Thread* t, int* out_code);
AETHER_API b8     thread_set_priority(Thread* t, ThreadPriority p);
AETHER_API void   thread_yield(void);
AETHER_API void   thread_sleep_ms(u32 ms);

#if AETHER_LANG_CPP
}
#endif // AETHER_LANG_CPP

#endif // AETHER_H_


/*---------------------------------------------------------------------------*/
#if defined(AETHER_IMPLEMENTATION) && !defined(AETHER_IMPLEMENTATION_DONE)
#define AETHER_IMPLEMENTATION_DONE

#include <string.h>
#include <stdarg.h>
#include <stdlib.h> /* for str8_to_f64 */
#include <errno.h>  /* for str8_to_f64 */

/*---------------------------------------------------------------------------*/
/* --- P L A T F O R M ----------------------------------------------------- */
/*---------------------------------------------------------------------------*/
#if AETHER_OS_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif // WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif // NOMINMAX

    #include <windows.h>
    #include <process.h> /* required for _beginthreadex */

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

#endif // AETHER_OS_WINDOWS

#if AETHER_LANG_CPP
extern "C"
{
#endif // AETHER_LANG_CPP

// internal u32 os_get_last_error(void)
// {
// #if AETHER_OS_WINDOWS
//     return (u32)GetLastError();
// #else
//     #error "AETHER: OS get last error not implemented on this platform"
// #endif
// }
//
// internal str8 os_error_string(Arena* arena, u32 code)
// {
// #if AETHER_OS_WINDOWS
//     char buf[512];
//     DWORD len = FormatMessageA(
//         FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
//         0, (DWORD)code,
//         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
//         buf, sizeof(buf), 0
//     );
//
//     // strip trailing "\r\n"
//     while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
//     {
//         len -= 1;
//     }
//
//     if (len == 0) return str8_push_fmt(arena, "unknown error (%u)", code );
//
//     return str8_push_copy(arena, view_from_raw(buf, len));
// #else
//     #error "AETHER: OS error string not implemented on this platform"
// #endif
// }

internal u64 os_mem_pagesize(void)
{
#if AETHER_OS_WINDOWS
    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    u64 pagesize = (u64)sysinfo.dwPageSize;
    return pagesize;
#else
    #error "AETHER: OS memory page size not implemented for this platform"
#endif
}

internal u64 os_mem_pagegranularity(void)
{
#if AETHER_OS_WINDOWS
    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);
    u64 page_granularity = (u64)sysinfo.dwAllocationGranularity;
    return page_granularity;
#else
    #error "AETHER: OS memory page granularity not implemented for this platform"
#endif
}

internal void* os_mem_reserve(u64 size)
{
#if AETHER_OS_WINDOWS
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
#else
    #error "AETHER: OS memory allocation not implemented for this platform"
#endif
}

internal b8 os_mem_commit(void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
    void* ret = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    return ret != NULL;
#else
    #error "AETHER: OS memory commit not implemented for this platform"
#endif
}

internal b8 os_mem_decommit(void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
    return (VirtualFree(ptr, size, MEM_DECOMMIT) != 0);
#else
    #error "AETHER: OS memory decommit not implemented for this platform"
#endif
}

internal b8 os_mem_release(void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
    (void)size;
    return (VirtualFree(ptr, 0, MEM_RELEASE) != 0);
#else
    #error "AETHER: OS memory release not implemented for this platform"
#endif
}

/* todo(chris): maybe we need an os_file_open_ex,
 *              or os_file_open that allows user
 *              defined read/write mode */

internal void* os_file_open_for_read(const char* path)
{
#if AETHER_OS_WINDOWS
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

internal void* os_file_open_for_write(const char* path)
{
#if AETHER_OS_WINDOWS
    HANDLE h = CreateFileA(
        path,
        GENERIC_WRITE,
        0,             /* no sharing while we write */
        NULL,
        CREATE_ALWAYS, /* create, or truncate existing */
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (h == INVALID_HANDLE_VALUE) return NULL;
    return h;
#else
    #error "AETHER: OS file open for write not implemented for this platform"
#endif
}

internal void os_file_close(void* handle)
{
#if AETHER_OS_WINDOWS
    CloseHandle(handle);
#else
    #error "AETHER: OS file close not implemented for this platform"
#endif
}

internal b8  os_file_size(void* handle, u64* out_size)
{
#if AETHER_OS_WINDOWS
    LARGE_INTEGER filesize;
    if (!GetFileSizeEx(handle, &filesize))
        return false;

    *out_size = (u64)filesize.QuadPart;
    return true;
#else
    #error "AETHER: OS file size not implemented for this platform"
#endif
}

internal b8  os_file_read(void* handle, void* dst, u64 size)
{
#if AETHER_OS_WINDOWS
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

internal b8 os_file_write(void* handle, const void* src, u64 size)
{
#if AETHER_OS_WINDOWS
    const u8* cursor    = (const u8*)src;
    u64       remaining = size;

    while (remaining > 0)
    {
        DWORD chunk = (remaining > MAXDWORD) ? MAXDWORD : (DWORD)remaining;
        DWORD written = 0;

        if (!WriteFile(handle, cursor, chunk, &written, NULL))
        {
            return false;
        }

        if (written == 0) /* no forward progress */
            return false;

        cursor    += written;
        remaining -= written;

    }
    return true;
    
#else
    #error "AETHER: OS file write not implemented on this platform"
#endif
}

internal void* os_file_map(void* handle, u64 size)
{
#if AETHER_OS_WINDOWS
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

internal void* os_mem_reserve_ring(u64 size)
{
#if AETHER_OS_WINDOWS
    VirtualAlloc2_fn va2 = (VirtualAlloc2_fn)GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "VirtualAlloc2");
    if (!va2) return NULL;
    return va2(NULL, NULL, 2*size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
#else
    #error "AETHER: OS memory ring allocation not implemented for this platform"
#endif
}

internal b8 os_mem_split_ring(void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
    return VirtualFree(ptr, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER) != 0;
#else
    #error "AETHER: OS memory ring splitting not implemented for this platform"
#endif
}

internal void* os_mem_map_ring(void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
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

internal b8 os_mem_release_ring(void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
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

internal void os_file_unmap(const void* ptr, u64 size)
{
#if AETHER_OS_WINDOWS
    (void)size;
    UnmapViewOfFile(ptr);
#else
    #error "AETHER: OS file unmap not implemented for this platform"
#endif
}

internal u64 os_time_now(void)
{
#if AETHER_OS_WINDOWS
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (u64)counter.QuadPart;
#else
    #error "AETHER: OS time now not implemented for this platform"
#endif
}

internal u64 os_time_frequency(void)
{
#if AETHER_OS_WINDOWS
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (u64)freq.QuadPart;
#else
    #error "AETHER: OS time frequency not implemented for this platform"
#endif
}

internal void* os_create_timer(void)
{
#if AETHER_OS_WINDOWS
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

internal void os_timer_sleep(void* h, u64 sleep_ticks)
{
#if AETHER_OS_WINDOWS

    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)(sleep_ticks * 10000000ull / os_time_frequency());
    if (!SetWaitableTimer((HANDLE)h, &due, 0, NULL, NULL, FALSE)) return;
    WaitForSingleObject(h, INFINITE);
    return;
#else
    #error "AETHER: OS set timer not implemented for this platform"
#endif
}

internal void os_timer_release(void* h)
{
#if AETHER_OS_WINDOWS
    CloseHandle(h);
#else
    #error "AETHER: OS release timer not implemented for this platform"
#endif
}

internal void os_cpu_relax(void)
{
#if AETHER_OS_WINDOWS
    YieldProcessor();
#else
    #error "AETHER: OS cpu relax not implemented for this platform"
#endif
}

internal void os_thread_yield(void)
{
#if AETHER_OS_WINDOWS
    SwitchToThread();
#else
    #error "AETHER: OS thread yield not implemented for this platform"
#endif

}
typedef struct ThreadStart_ { thread_fn fn; void* user; u64 taken; } ThreadStart_;

#if AETHER_OS_WINDOWS
internal unsigned __stdcall os_thread_thunk_(void* arg)
{
    ThreadStart_* start = (ThreadStart_*)arg;
    thread_fn fn = start->fn;
    void* user   = start->user;
    atomic_store_rel_u64(&start->taken, 1);
    return (unsigned)fn(user);
}
#endif // AETHER_OS_WINDOWS

internal void* os_thread_create(thread_fn fn, void* user)
{
    ThreadStart_ start = {0};
    start.fn   = fn;
    start.user = user;
    void* h = NULL;
#if AETHER_OS_WINDOWS
    h = (void*)_beginthreadex(NULL, 0, os_thread_thunk_, &start, 0, NULL);
#else
    #error "AETHER: OS thread create not implemented on this platform"
#endif
    if (!h) return NULL;
    while (!atomic_load_acq_u64(&start.taken)) os_thread_yield();
    return h;
}

internal int os_thread_join(void* h)
{
#if AETHER_OS_WINDOWS
    WaitForSingleObject((HANDLE)h, INFINITE);
    DWORD code = 0;
    GetExitCodeThread((HANDLE)h, &code);
    CloseHandle((HANDLE)h);
    return (int)code;
#else
    #error "AETHER: OS thread join not implemented on this platform"
#endif
}

internal b8 os_thread_set_priority(void* h, ThreadPriority p)
{
#if AETHER_OS_WINDOWS
    int win = THREAD_PRIORITY_NORMAL;
    if (p == ThreadPriority_High)         win = THREAD_PRIORITY_HIGHEST;
    if (p == ThreadPriority_TimeCritical) win = THREAD_PRIORITY_TIME_CRITICAL;
    return SetThreadPriority((HANDLE)h, win) != 0;
#else
    #error "AETHER: OS set thread priority not implemented on this platform"
#endif
}

internal void os_thread_sleep_ms(u32 ms)
{
#if AETHER_OS_WINDOWS
    if (ms == 0) { os_thread_yield(); return; }
    HANDLE h = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!h) { Sleep(ms); return; } /* pre-1803: flag is rejected */
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)ms * 10000; /* relative, 100 ns units */
    if (SetWaitableTimer(h, &due, 0, NULL, NULL, FALSE))
        WaitForSingleObject(h, INFINITE);
    else
        Sleep(ms);
    CloseHandle(h);
#else
    #error "AETHER: OS thread sleep ms not implemented on this platform"
#endif
}

/* ------------------------------------------------------------------------- */
/* --- A R E N A S --------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

internal u64 round_up_power_2(u64 n)
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

internal u64 align_forward_u64(u64 value, u64 align)
{
    AETHER_ASSERT_(align != 0);
    AETHER_ASSERT_((align & (align - 1)) == 0);

    u64 mask = align - 1;
    return (value + mask) & ~mask;
}

internal void arena_commit_page_or_chunk(Arena* arena, u64 new_pos)
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

internal void arena_decommit_tail(Arena* arena, u64 new_pos)
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

AETHER_API Arena* arena_alloc_ex(u64 reserve_size, u64 initial_commit_size, u32 commit_page_granularity, ArenaFlags flags)
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

AETHER_API Arena* arena_alloc(u64 reserve_size)
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

AETHER_API void arena_release(Arena* arena)
{
    if (!arena) return;
    os_mem_release(arena, arena->reserved_size);
}

AETHER_API void* arena_push(Arena* arena, u64 size, u64 align, ArenaZero zero)
{
    AETHER_ASSERT_(arena->pos <= arena->reserved_size);

    u64 aligned_pos = align_forward_u64(arena->pos, align);
    if (aligned_pos < arena->pos || aligned_pos > arena->reserved_size || size > arena->reserved_size - aligned_pos)
    {
        AETHER_ASSERT_(!"arena_push overflow"); return NULL;
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

AETHER_API void arena_pop_to(Arena* arena, u64 pos)
{
    if (pos < AETHER_ARENA_HEADER_SIZE) pos = AETHER_ARENA_HEADER_SIZE;
    if (pos > arena->pos)               pos = arena->pos;

    if ((arena->flags & ArenaFlags_DebugFillOnClear) != 0)
        memset((u8*)arena + pos, 0xDD, arena->pos - pos);

    if ((arena->flags & ArenaFlags_Decommit) != 0)
        arena_decommit_tail(arena, pos);

    arena->pos = pos;
}


AETHER_API void arena_pop(Arena* arena, u64 amt)
{
    if (amt > arena->pos)
        amt = arena->pos;

    arena_pop_to(arena, arena->pos - amt);
}

AETHER_API void arena_clear(Arena* arena)
{
    arena_pop_to(arena, AETHER_ARENA_HEADER_SIZE);
}

AETHER_API ArenaTemp arena_begin_temp(Arena* arena)
{
    ArenaTemp tmp;
    tmp.arena = arena;
    tmp.pos   = arena->pos;
    return tmp;
}

AETHER_API void arena_end_temp(ArenaTemp temp)
{
    AETHER_ASSERT_(temp.pos <= temp.arena->pos);
    arena_pop_to(temp.arena, temp.pos);
}

/* ------------------------------------------------------------------------- */
/* --- R I N G / B U F F E R S --------------------------------------------- */
/* ------------------------------------------------------------------------- */

AETHER_API RingBuffer ring_buffer_alloc(u64 size)
{
    AETHER_ASSERT_(size > 0);

    RingBuffer rb = {0};

    u64 size_pow2        = round_up_power_2(size);
    u64 page_granularity = os_mem_pagegranularity(); /* should already be power of 2 */
    u64 ring_size        = AETHER_MAX_(size_pow2, page_granularity);

    rb.base = (u8*)os_mem_reserve_ring(ring_size);
    if (!rb.base) FATAL("Failed to allocate RingBuffer\n");

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

AETHER_API void ring_buffer_release(RingBuffer* rb)
{
    if (!rb || !rb->base) return;
    os_mem_release_ring(rb->base, rb->size);
    rb->base  = NULL;
    rb->size  = 0;
    rb->read  = 0;
    rb->write = 0;
}

AETHER_API u64 ring_buffer_available(RingBuffer* rb)
{
    if (!rb || !rb->base) return 0;
    u64 read  = atomic_load_acq_u64(&rb->read);
    u64 write = atomic_load_acq_u64(&rb->write);
    return write - read;
}

AETHER_API b8 ring_buffer_read(RingBuffer* rb, void* dst, u64 len)
{
    if (!rb || !rb->base) return false;
    if (len == 0) return true;

    bytes_view view = ring_buffer_peek(rb, len);
    if (!view.size) return false;

    memcpy(dst, view.data, view.size);
    ring_buffer_advance_read(rb, view.size);

    return true;
}

AETHER_API b8 ring_buffer_write(RingBuffer* rb, const void* src, u64 len)
{
    if (!rb || !rb->base) return false;
    if (len > rb->size ) return false;

    u64 read  = atomic_load_acq_u64(&rb->read);
    u64 write = rb->write;

    /* reject if it will overwrite unread data */
    if (len > rb->size - (write - read)) return false;

    memcpy(rb->base + (write & (rb->size - 1)), src, len);

    atomic_store_rel_u64(&rb->write, write + len);
    return true;
}

AETHER_API b8  ring_buffer_advance_read(RingBuffer* rb, u64 len)
{
    if (!rb || !rb->base) return false;

    u64 write = atomic_load_acq_u64(&rb->write);
    u64 read  = rb->read;

    if (len > write - read) return false;

    atomic_store_rel_u64(&rb->read, read + len);

    return true;
}

AETHER_API bytes_view ring_buffer_peek(RingBuffer* rb, u64 len)
{
    bytes_view v = {0};
    if (!rb || !rb->base) return v;

    u64 write = atomic_load_acq_u64(&rb->write);
    u64 read  = rb->read;

    if (len <= write - read)
    {
        v.data = rb->base + (read & (rb->size - 1));
        v.size = len;
    }
    return v;
}

/* ------------------------------------------------------------------------- */
/* --- S T R I N G S ------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
internal void* arena_push_or_fatal_(Arena* arena, u64 size, u64 align)
{
    void* p = arena_push(arena, size, align, ArenaZero_Never);
    if (!p) FATAL("arena exhausted");
    return p;
}

AETHER_API char* c_str(Arena* arena, str8_view s)
{
    AETHER_ASSERT_(arena != NULL);
    AETHER_ASSERT_(s.data != NULL || s.size == 0); /* NULL data with size > 0 is a caller bug */

    char* dst = (char*)arena_push_or_fatal_(arena, s.size + 1, 1);
    if (s.size) memcpy(dst, s.data, s.size);
    dst[s.size] = '\0';
    return dst;
}

AETHER_API char* c_str_push_copy(Arena* arena, const char* src)
{
    size_t len = strlen(src);
    char *dst = (char*)arena_push_or_fatal_(arena, (u64)len + 1, 1);
    memcpy(dst, src, len + 1);
    return dst;
}

static inline char* c_str_push_fmtv(Arena* arena, const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    AETHER_ASSERT_(len >= 0);

    /* use 1-byte alignment to maximum packing */
    char* dst = (char*)arena_push_or_fatal_(arena, (u64)len + 1, 1);
    int written = vsnprintf(dst, (size_t)len + 1, fmt, args);
    AETHER_ASSERT_(written == len);
    (void)written;

    return dst;
}

AETHER_API char* c_str_push_fmt(Arena* arena, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char* dst = c_str_push_fmtv(arena, fmt, args);
    va_end(args);
    return dst;
}

AETHER_API str8 str8_push_copy(Arena* arena, str8_view src)
{
    AETHER_ASSERT_(arena != NULL);
    AETHER_ASSERT_(src.data != NULL || src.size == 0); /* NULL data with size > 0 is a caller bug */

    str8 result;
    result.size = src.size;
    result.data = (u8*)arena_push_or_fatal_(arena, src.size + 1, 1);
    if (src.size) memcpy(result.data, src.data, src.size);
    result.data[result.size] = '\0';
    return result;
}

AETHER_API str8 str8_push_c_str(Arena* arena, const char* src)
{
    size_t len = strlen(src);
    char*  dst = (char*)arena_push_or_fatal_(arena, (u64)len + 1, 1);
    memcpy(dst, src, len + 1);

    str8 result;
    result.data = (u8*)dst;
    result.size = (u64)len; /* excludes the null terminator */
    return result;
}

static inline str8 str8_push_fmtv(Arena* arena, const char* fmt, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, fmt, args_copy);

    va_end(args_copy);

    AETHER_ASSERT_(len >= 0);

    char* dst = (char*)arena_push_or_fatal_(arena, (u64)len + 1, 1);

    int written = vsnprintf(dst, (size_t)len + 1, fmt, args);
    AETHER_ASSERT_(written == len);
    (void)written;

    str8 result;
    result.data = (u8*)dst;
    result.size = (u64)len; /* excludes null terminator */
    return result;
}

AETHER_API str8 str8_push_fmt(Arena* arena, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str8 result = str8_push_fmtv(arena, fmt, args);
    va_end(args);
    return result;
}

AETHER_API str8 str8_concat(Arena* arena, str8_view a, str8_view b)
{
    AETHER_ASSERT_(arena != NULL);
    AETHER_ASSERT_(a.data != NULL || a.size == 0); /* NULL data with size > 0 is a caller bug */
    AETHER_ASSERT_(b.data != NULL || b.size == 0); /* NULL data with size > 0 is a caller bug */

    str8 result;

    result.size = a.size + b.size;
    result.data = (u8*)arena_push_or_fatal_(arena, result.size+1, 1);
    if (a.size) memcpy(result.data, a.data, a.size);
    if (b.size) memcpy(result.data+a.size, b.data, b.size);
    result.data[result.size] = '\0';

    return result;
}

AETHER_API str8_view view_from_c_str(const char* s)
{
    str8_view v = {0};

    if (!s) return v;

    u64 len = (u64)strlen(s);
    if (!len) return v;

    v.data = (const u8*)s;
    v.size = len;

    return v;
}

AETHER_API str8_view str8_slice(str8_view s, u64 start, u64 end)
{
    AETHER_ASSERT_(start <= end && end <= s.size);
    str8_view v;
    v.data = s.data + start;
    v.size = end - start;
    return v;
}

AETHER_API str8_view str8_skip(str8_view s, u64 n)
{
    AETHER_ASSERT_(n <= s.size);
    return str8_slice(s, n, s.size);
}

AETHER_API str8_view str8_drop(str8_view s, u64 n)
{
    AETHER_ASSERT_(n <= s.size);
    return str8_slice(s, 0, s.size - n);
}

internal b8 char_is_ws(u8 c)    { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
internal b8 char_is_upper(u8 c) { return ('A' <= c && c <= 'Z'); }
internal b8 char_is_lower(u8 c) { return ('a' <= c && c <= 'z'); }

AETHER_API str8_view str8_trim(str8_view s)
{
    u64 start = 0;
    while (start < s.size && char_is_ws(s.data[start])) start++;

    u64 end   = s.size;
    while (end > start && char_is_ws(s.data[end-1])) end--;

    return str8_slice(s, start, end);
}

AETHER_API str8_view str8_trim_left(str8_view s)
{
    u64 start = 0;
    while (start < s.size && char_is_ws(s.data[start])) start++;

    return str8_slice(s, start, s.size);
}

AETHER_API str8_view str8_trim_right(str8_view s)
{
    u64 end = s.size;
    while ( end > 0 && char_is_ws(s.data[end-1])) end--;

    return str8_slice(s, 0, end);
}


AETHER_API b8 str8_eq(str8_view a, str8_view b)
{
    AETHER_ASSERT_(a.data != NULL || a.size == 0); /* NULL data with size > 0 is a caller bug */
    AETHER_ASSERT_(b.data != NULL || b.size == 0); /* NULL data with size > 0 is a caller bug */

    if (a.size != b.size) return false;
    if (a.size == 0)      return true;
    if (a.data == b.data) return true;
    return memcmp(a.data, b.data, a.size) == 0;
}

AETHER_API b8 str8_eq_nocase(str8_view a, str8_view b)
{
    if (a.size != b.size) return false;

    u8 c1, c2;
    for (u64 i = 0; i < a.size; ++i)
    {
        c1 = char_is_upper(a.data[i]) ? a.data[i] + 32 : a.data[i];
        c2 = char_is_upper(b.data[i]) ? b.data[i] + 32 : b.data[i];
        if (c1 != c2) return false;
    }
    return true;

}

AETHER_API b8 str8_has_prefix(str8_view s, str8_view prefix)
{
    if (!prefix.data || !prefix.size || !s.data || !s.size) return false;

    if (s.size < prefix.size) return false;

    for (u64 i = 0; i < prefix.size; ++i)
        if (s.data[i] != prefix.data[i]) return false;

    return true;
}

AETHER_API b8 str8_has_suffix(str8_view s, str8_view suffix)
{
    if (!suffix.data || !suffix.size || !s.data || !s.size) return false;

    if (s.size < suffix.size) return false;

    u64 j = 0;
    for (u64 i = s.size - suffix.size; i < s.size; ++i)
    {
        if (j >= suffix.size) return false;
        if (s.data[i] != suffix.data[j++]) return false;
    }

    return true;
}

// todo(chris): update brute-force method to use Boyer-Moore-Horspool
AETHER_API b8 str8_find(str8_view s, str8_view needle, u64* pos)
{
    if (needle.size == 0) { *pos = 0; return true; }
    if (needle.size > s.size) return false;

    u64 last = s.size - needle.size;
    for (u64 i = 0; i <= last; i += 1)
    {
        u64 j = 0;
        while (j < needle.size && s.data[i+j] == needle.data[j]) { j += 1; }
        if (j == needle.size) { *pos = (u64)i; return true; }
    }
    return false;
}

AETHER_API b8 str8_find_last(str8_view s, str8_view needle, u64* pos)
{
    if (needle.size == 0) {*pos = s.size; return true;}
    b8 found = false;
    u64 base = 0;
    str8_view rest = s;
    u64 p;
    while (rest.size > 0 && str8_find(rest, needle, &p))
    {
        found = true;
        *pos  = base + p;
        base += p + needle.size;
        rest = str8_skip(s, base);
    }
    return found;
}

AETHER_API b8 str8_find_char(str8_view s, u8 c, u64* pos)
{
    for (u64 i = 0; i < s.size; ++i)
    {
        if (c == s.data[i]) { *pos = i; return true; }
    }
    return false;
}

AETHER_API i32 str8_cmp(str8_view a, str8_view b)
{
    AETHER_ASSERT_(a.data != NULL || a.size == 0); /* NULL data with size > 0 is a caller bug */
    AETHER_ASSERT_(b.data != NULL || b.size == 0); /* NULL data with size > 0 is a caller bug */

    u64 n = AETHER_MIN_(a.size, b.size);
    i32 r = n ? memcmp(a.data, b.data, (size_t)n) : 0;
    if (r != 0) return r;
    if (a.size != b.size) return (a.size < b.size) ? -1 : 1;
    return 0;
}

AETHER_API b8 str8_cut(str8_view s, str8_view sep, str8_view* before, str8_view* after)
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

AETHER_API Str8List str8_split(Arena* arena, str8_view s, str8_view sep, Str8SplitFlags flags)
{
    Str8List list = {0};

    str8_view before;
    str8_view rest = s;
    b8 more;
    do {
        more = str8_cut(rest, sep, &before, &rest);
        if (before.size > 0 || !(flags & Str8SplitFlags_SkipEmpty))
            str8_list_push(arena, &list, before);
    } while (more);
    return list;
}

AETHER_API void str8_list_push(Arena* arena, Str8List* list, str8_view v)
{
    Str8Node* node = arena_push_t_nozero(arena, Str8Node);
    if (!node) FATAL("arena exhausted");
    node->v = v;
    node->next = 0;

    if (list->last) list->last->next = node;
    else            list->first      = node;

    list->last       = node;
    list->count     += 1;
    list->total_len += v.size;
}

internal void str8_list_push_fmtv(Arena* arena, Str8List* list, const char* fmt, va_list args)
{
    str8_view v = view_from_str8(str8_push_fmtv(arena, fmt, args));
    str8_list_push(arena, list, v);
}

AETHER_API void str8_list_push_fmt(Arena* arena, Str8List* list, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str8_list_push_fmtv(arena, list, fmt, args);
    va_end(args);
}

AETHER_API str8 str8_join(Arena* arena, Str8List* list, str8_view sep)
{
    str8 dst = {0};

    Str8Node* node = list->first;
    if (!node) return dst;

    u64 len = list->total_len + (list->count - 1) * sep.size;
    u8* buf = (u8*)arena_push_or_fatal_(arena, len + 1, 1);

    u64 offset = 0;
    while (node)
    {
        if (node != list->first)
        {
            memcpy(buf + offset, sep.data, sep.size);
            offset += sep.size;
        }
        memcpy(buf + offset, node->v.data, node->v.size);
        offset += node->v.size;
        node = node->next;

    }
    buf[len] = '\0';

    dst.data = buf;
    dst.size = len;
    return dst;
}

AETHER_API Str8Array str8_list_to_array(Arena* arena, Str8List* list)
{
    Str8Array result = {0};
    if (!list) return result;

    result.items = (str8_view*)arena_push_array_nozero(arena, str8_view, list->count);
    if (!result.items) FATAL("arena exhausted");
    result.count = list->count;

    Str8Node* node = list->first;
    for (u64 i = 0; i < result.count; ++i)
    {
        result.items[i] = node->v;
        node = node->next;
        AETHER_ASSERT_(node);
    }
    return result;
}

AETHER_API str8 str8_to_upper(Arena* arena, str8_view s)
{
    str8 result;
    result.size = s.size;
    result.data = (u8*)arena_push_or_fatal_(arena, s.size + 1, 1);

    for (u64 i = 0; i < s.size; ++i)
        result.data[i] = char_is_lower(s.data[i]) ? s.data[i] - 32 : s.data[i];

    result.data[result.size] = '\0';
    return result;
}

AETHER_API str8 str8_to_lower(Arena* arena, str8_view s)
{
    str8 result;
    result.size = s.size;
    result.data = (u8*)arena_push_or_fatal_(arena, s.size + 1, 1);

    for (u64 i = 0; i < s.size; ++i)
        result.data[i] = char_is_upper(s.data[i]) ? s.data[i] + 32: s.data[i];

    result.data[result.size] = '\0';
    return result;
}

AETHER_API str8 str8_replace(Arena* arena, str8_view s, str8_view old, str8_view target)
{

    u64 count = 0;
    u64 pos   = 0;

    str8_view rest = s;
    while (old.size > 0 && old.size<= rest.size && str8_find(rest, old, &pos))
    {
        count += 1;
        rest = str8_skip(rest, pos + old.size);
    }

    u64 new_len = s.size + count * (i64)(target.size - old.size);
    u8* buf     = (u8*)arena_push_or_fatal_(arena, new_len + 1, 1);

    u64 w = 0;
    rest = s;
    while (old.size > 0 && old.size <= rest.size && str8_find(rest, old, &pos))
    {
        memcpy(buf + w, rest.data, pos); w += pos;
        memcpy(buf + w, target.data, target.size); w += target.size;
        rest = str8_skip(rest, pos + old.size);
    }

    memcpy(buf + w, rest.data, rest.size); w += rest.size;
    buf[w] = '\0';

    str8 result = {buf, w};
    return result;
}


AETHER_API b8 str8_to_u64(str8_view s, u64* out)
{
    if (s.size == 0) return false;

    u64 v = 0;
    b8 has_digit = false;
    for (u64 i = 0; i < s.size; ++i)
    {
        u8 c = s.data[i];
        if (i == 0 && c == '-') return false;
        if (i == 0 && c == '+') continue;
        if (c < '0' || c > '9') return false;

        u8 d = c - '0';

        if ( v > (U64_MAX - d) / 10 ) return false; /* would overflow */
        v = v * 10 + d;
        has_digit = true;
    }

    if (!has_digit) return false;

    *out = v;
    return true;
}

AETHER_API b8 str8_to_i64(str8_view s, i64* out)
{
    if (s.size == 0) return false;

    u64 v = 0;
    i64 sign = 1;
    b8 has_digit = false;
    for (u64 i = 0; i < s.size; ++i) 
    {
        u8 c = s.data[i];

        if (i == 0 && c == '-') { sign = -1; continue; }
        if (i == 0 && c == '+') continue;
        if (c < '0' || c > '9') return false;

        u8 d = c - '0';

        if ( v > (U64_MAX - d) / 10 ) return false;
        v = v * 10 + d;
        has_digit = true;
    }
    
    if ( !has_digit ) return false;
    if ( sign ==  1 && v > (u64)I64_MAX)     return false;
    if ( sign == -1 && v > (u64)I64_MAX + 1) return false;

    *out = (i64)(sign == -1 ? (u64)0 - v : v);
    return true;
}

AETHER_API b8 str8_to_f64(str8_view s, f64* out)
{
    // todo(chris): handroll this for the excercise
    char buf[64];
    if (s.size == 0 || s.size >= sizeof(buf)) return false;

    memcpy(buf, s.data, s.size);
    buf[s.size] = '\0';

    char* end = NULL;
    errno = 0;
    f64 v = strtod(buf, &end);

    if (end != buf + s.size) return false; /* didn't consume the whole string */
    if (errno == ERANGE)     return false; /* overflow to +/- HUGE_VAL, or subnormal underflow */

    *out = v;
    return true;
}

/* ------------------------------------------------------------------------- */
/* --- F I L E - I / O ----------------------------------------------------- */
/* ------------------------------------------------------------------------- */


AETHER_API bytes  file_read(Arena* arena, const char* path)
{
    bytes result = {0};
    void* h = os_file_open_for_read(path);
    if (!h) return result;

    u64 size = 0;
    if (!os_file_size(h, &size)) { os_file_close(h); return result; } /* recoverable: missing/inaccessible file */

    u64 mark = arena->pos;

    u8* data = arena_push_array_nozero(arena, u8, size);
    if (!data)
    {
        os_file_close(h);
        arena_pop_to(arena, mark);
        return result;
    }

    b8 ok = os_file_read(h, data, size);
    os_file_close(h);

    if (!ok) {
        arena_pop_to(arena, mark);
        return result;
    }

    result.data = data;
    result.size = size;
    return result;
}

AETHER_API u64 file_write(const char* path, bytes_view data)
{
    void* h = os_file_open_for_write(path);
    if (!h) return 0;

    b8 ok = os_file_write(h, data.data, data.size);
    os_file_close(h);

    return ok ? data.size : 0;
}

AETHER_API bytes_view  file_map(const char* path)
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

AETHER_API void  file_unmap(bytes_view buf)
{
    os_file_unmap(buf.data, buf.size);
}

/* ------------------------------------------------------------------------- */
/* --- T I M I N G --------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

AETHER_API u64 time_mark(void)
{
    return os_time_now();
}

AETHER_API f64 time_elapsed_sec(u64 start, u64 end)
{
    return (f64)(end - start) / (f64)os_time_frequency();
}


AETHER_API HighResTimer high_res_timer_create(f64 hz)
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

AETHER_API u64 high_res_timer_wait(HighResTimer* t)
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

    if (t->next_deadline > now + t->spin_margin)
        os_timer_sleep(t->os_timer, t->next_deadline - now - t->spin_margin);

    while (os_time_now() < t->next_deadline)
        os_cpu_relax();

    return 0;
}

AETHER_API void high_res_timer_release(HighResTimer* t)
{
    if (!t || !t->os_timer) return;
    os_timer_release(t->os_timer);
    t->os_timer      = NULL;
    t->period_ticks  = 0;
    t->next_deadline = 0;
    t->overrun       = 0;
    return;
}
/*---------------------------------------------------------------------------*/
/* --- T H R E A D S ------------------------------------------------------- */
/*---------------------------------------------------------------------------*/

AETHER_API Thread thread_create(thread_fn fn, void* user)
{
    Thread t = {0};
    if (!fn) return t;
    t.handle = os_thread_create(fn, user);
    return t;
}

AETHER_API b8 thread_join(Thread* t, int* out_code)
{
    if (!t || !t->handle) return false;
    int code = os_thread_join(t->handle);
    t->handle = NULL;
    if (out_code) *out_code = code;
    return true;
}

AETHER_API b8 thread_set_priority(Thread* t, ThreadPriority p)
{
    if (!t || !t->handle) return false;
    return os_thread_set_priority(t->handle, p);
}

AETHER_API void thread_yield(void)
{
   os_thread_yield();
}

AETHER_API void thread_sleep_ms(u32 ms)
{
    os_thread_sleep_ms(ms);
}


#if AETHER_LANG_CPP
}
#endif // AETHER_LANG_CPP

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
