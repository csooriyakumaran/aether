#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* AETHER_NO_ASSERT tells aether.h not to claim the ASSERT name, so we define
   our own here: it records a failure and keeps going instead of trapping on
   the first one (the library's own ASSERT is a debugger trap in debug builds
   and a no-op in release, either of which would break a test run). This also
   doubles as a compile-time check that the AETHER_NO_ASSERT opt-out works. */

static int g_checks   = 0;
static int g_failures = 0;

static int section_checks_start     = 0;
static int section_failures_start  = 0;
static const char* current_section = NULL;

#define ASSERT(cond) do {                                                   \
    g_checks++;                                                             \
    if (!(cond)) {                                                          \
        g_failures++;                                                       \
        fprintf(stderr, "  FAIL: %s [%s:%d]\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static void section_summary_flush(void)
{
    if (!current_section) return;
    int total  = g_checks - section_checks_start;
    int failed = g_failures - section_failures_start;
    printf("   [%d/%d passed]\n", total - failed, total);
}

// #define SECTION(name) printf("-- %s\n", name)
#define SECTION(name) do {               \
    section_summary_flush();             \
    current_section        = name;       \
    section_checks_start   = g_checks;   \
    section_failures_start = g_failures; \
    printf("-- %s\n", name);             \
} while (0)

static b8 bytes_all_eq(const u8* p, u64 n, u8 value)
{
    for (u64 i = 0; i < n; ++i)
        if (p[i] != value) return false;
    return true;
}

static u64 page_align_up(u64 value, u64 pagesize)
{
    return ((value + pagesize - 1) / pagesize) * pagesize;
}

static void test_array_count(void)
{
    SECTION("ARRAY_COUNT: element count for fixed-size arrays");

    int   ints[5]    = {0};
    f64   doubles[3] = {0};
    u8    raw[7]     = {0};
    Arena arenas[2]  = {0}; /* array of structs */
    char  one[1]     = {0};

    ASSERT(ARRAY_COUNT(ints)    == 5);
    ASSERT(ARRAY_COUNT(doubles) == 3);
    ASSERT(ARRAY_COUNT(raw)     == 7);
    ASSERT(ARRAY_COUNT(arenas)  == 2);
    ASSERT(ARRAY_COUNT(one)     == 1);

    /* count is independent of element size */
    ASSERT(sizeof(doubles) == ARRAY_COUNT(doubles) * sizeof(doubles[0]));

    /* ARRAY_COUNT is a compile-time integer constant expression: if it weren't,
       neither the _Static_assert nor the array bound below would compile. */
    _Static_assert(ARRAY_COUNT(ints) == 5, "ARRAY_COUNT must be a constant expression");

    int as_bound[ARRAY_COUNT(ints)]; /* not a VLA -- sized at compile time */
    ASSERT(sizeof(as_bound) == sizeof(ints));

    /* the private name resolves identically (uniform with MIN_/ASSERT_) */
    ASSERT(AETHER_ARRAY_COUNT_(ints) == ARRAY_COUNT(ints));
}

static void test_alloc_basic(void)
{
    SECTION("alloc: basic reserve/commit/pos state");

    u64 pagesize = os_mem_pagesize();
    Arena arena = arena_alloc(MB(1));

    ASSERT(arena.base != 0);
    ASSERT(arena.pos == 0);
    ASSERT(arena.commit_size == 0);
    ASSERT(arena.reserved_size == page_align_up(MB(1), pagesize));
    ASSERT(arena.granularity == 1);

#if AETHER_ENABLE_ASSERTS
    ASSERT(arena.flags == (ArenaFlags_AlwaysZero | ArenaFlags_DebugFillOnClear | ArenaFlags_Decommit));
#else
    ASSERT(arena.flags == ArenaFlags_None);
#endif

    arena_release(&arena);
}

static void test_alloc_ex_custom(void)
{
    SECTION("alloc_ex: custom reserve/commit/granularity/flags, and commit clamped to reserve");

    u64 pagesize = os_mem_pagesize();
    Arena arena = arena_alloc_ex(MB(1), KB(4), 4, ArenaFlags_CommitChunked);

    ASSERT(arena.base != 0);
    ASSERT(arena.reserved_size == page_align_up(MB(1), pagesize));
    ASSERT(arena.commit_size == page_align_up(KB(4), pagesize));
    ASSERT(arena.granularity == 4);
    ASSERT(arena.flags == ArenaFlags_CommitChunked);
    ASSERT(arena.pos == 0);

    /* requesting more initial commit than the reservation clamps, doesn't overflow it */
    Arena clamped = arena_alloc_ex(KB(4), MB(1), 1, ArenaFlags_None);
    ASSERT(clamped.commit_size == clamped.reserved_size);

    arena_release(&arena);
    arena_release(&clamped);
}

static void test_push_alignment(void)
{
    SECTION("push: returned pointers respect type alignment");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    u8* a = arena_push_t(&arena, u8);
    u8* b = arena_push_t(&arena, u8);
    u8* c = arena_push_t(&arena, u8);
    (void)a; (void)b; (void)c; /* just de-align pos before the next push */

    u64* d = arena_push_t(&arena, u64);
    ASSERT(((uintptr_t)d % ARENA_ALIGN(u64)) == 0);

    f64* e = arena_push_array(&arena, f64, 16);
    ASSERT(((uintptr_t)e % ARENA_ALIGN(f64)) == 0);

    arena_release(&arena);
}

static void test_push_array_contiguous(void)
{
    SECTION("push_array: contiguous, correctly sized, no overlap between pushes");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    u32* first = arena_push_array(&arena, u32, 128);
    for (u32 i = 0; i < 128; ++i) first[i] = i;

    u32* second = arena_push_array(&arena, u32, 64);
    for (u32 i = 0; i < 64; ++i) second[i] = 1000 + i;

    ASSERT((u8*)second == (u8*)(first + 128));

    b8 first_intact = true;
    for (u32 i = 0; i < 128; ++i)
        if (first[i] != i) { first_intact = false; break; }
    ASSERT(first_intact);

    b8 second_ok = true;
    for (u32 i = 0; i < 64; ++i)
        if (second[i] != 1000 + i) { second_ok = false; break; }
    ASSERT(second_ok);

    arena_release(&arena);
}

static void test_zero_policy(void)
{
    SECTION("push: zero policy (default vs _zero vs AlwaysZero flag)");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    u32* values = arena_push_array(&arena, u32, 8);
    for (u32 i = 0; i < 8; ++i) values[i] = 0xAAAAAAAAu;

    u64 mark = arena.pos - sizeof(u32) * 8;
    arena_pop_to(&arena, mark); /* no Decommit: committed bytes stay resident */

    u32* replay = arena_push_array(&arena, u32, 8); /* default push: must NOT zero */
    ASSERT(replay[0] == 0xAAAAAAAAu);

    arena_pop_to(&arena, mark);
    u32* forced_zero = arena_push_array_zero(&arena, u32, 8); /* explicit _zero: always zeroes */
    ASSERT(bytes_all_eq((const u8*)forced_zero, sizeof(u32) * 8, 0));

    arena_release(&arena);

    /* same dance, but the AlwaysZero policy flag should force zeroing on the default push too */
    Arena always_zero = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_AlwaysZero);

    u32* v2 = arena_push_array(&always_zero, u32, 8);
    for (u32 i = 0; i < 8; ++i) v2[i] = 0xBBBBBBBBu;

    u64 mark2 = always_zero.pos - sizeof(u32) * 8;
    arena_pop_to(&always_zero, mark2);

    u32* v2_replay = arena_push_array(&always_zero, u32, 8); /* default push, but policy forces zero */
    ASSERT(bytes_all_eq((const u8*)v2_replay, sizeof(u32) * 8, 0));

    arena_release(&always_zero);
}

static void test_pop_to_and_pop(void)
{
    SECTION("pop_to / pop: mark restore and clamping");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    u64 mark0 = arena.pos;
    arena_push_array(&arena, u8, 64);
    u64 mark1 = arena.pos;
    arena_push_array(&arena, u8, 64);
    u64 mark2 = arena.pos;

    arena_pop_to(&arena, mark1);
    ASSERT(arena.pos == mark1);

    arena_pop_to(&arena, mark0);
    ASSERT(arena.pos == mark0);

    /* popping to a position ahead of the current pos is clamped, not honored */
    arena_push_array(&arena, u8, 32);
    u64 before = arena.pos;
    arena_pop_to(&arena, mark2); /* mark2 > current pos */
    ASSERT(arena.pos == before);

    arena_pop(&arena, 16);
    ASSERT(arena.pos == before - 16);

    arena_pop(&arena, (u64)-1); /* absurdly large amount clamps at 0, doesn't underflow */
    ASSERT(arena.pos == 0);

    arena_release(&arena);
}

static void test_temp_basic(void)
{
    SECTION("temp: begin/end restores pos and the space is reusable");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    ArenaTemp temp = arena_begin_temp(&arena);
    u32* scratch = arena_push_array(&arena, u32, 64);
    for (u32 i = 0; i < 64; ++i) scratch[i] = 0xAAAAAAAAu;
    ASSERT(arena.pos != temp.pos);

    arena_end_temp(temp);
    ASSERT(arena.pos == temp.pos);

    u32* reused = arena_push_array(&arena, u32, 64); /* same bytes as scratch */
    ASSERT(reused == scratch);
    ASSERT(reused[0] == 0xAAAAAAAAu); /* default push: must NOT zero leftover content */

    arena_release(&arena);
}

static void test_temp_nested(void)
{
    SECTION("temp: nested begin/end pairs unwind in LIFO order");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);
    u64 base = arena.pos;

    ArenaTemp outer = arena_begin_temp(&arena);
    arena_push_array(&arena, u8, 64);
    u64 after_outer_push = arena.pos;

    ArenaTemp inner = arena_begin_temp(&arena);
    arena_push_array(&arena, u8, 32);
    ASSERT(arena.pos == after_outer_push + 32);

    arena_end_temp(inner);
    ASSERT(arena.pos == after_outer_push);

    arena_end_temp(outer);
    ASSERT(arena.pos == base);

    arena_release(&arena);
}

static void test_temp_out_of_order_is_safe(void)
{
    SECTION("temp: ending an outer temp first leaves a stale inner temp harmless");

#if AETHER_ENABLE_ASSERTS
    printf("   skipped: ASSERT() traps on the stale-temp check in this build (AETHER_ENABLE_ASSERTS=1);\n"
           "            rebuild with NDEBUG defined to exercise the release-mode failure path.\n");
#else
    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);
    u64 base = arena.pos;

    ArenaTemp outer = arena_begin_temp(&arena);
    arena_push_array(&arena, u8, 64);

    ArenaTemp inner = arena_begin_temp(&arena);
    arena_push_array(&arena, u8, 32);

    arena_end_temp(outer); /* ends first; pos rolls back past inner's mark */
    ASSERT(arena.pos == base);

    arena_end_temp(inner); /* stale: inner.pos > arena.pos now -- must clamp, not grow */
    ASSERT(arena.pos == base);

    arena_release(&arena);
#endif
}

static void test_temp_rk4_pattern(void)
{
    SECTION("temp: RK-stage pattern -- outer buffers survive repeated inner begin/end cycles");

    Arena scratch = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);
    u64 base = scratch.pos;
    u64 n    = 8;

    ArenaTemp step = arena_begin_temp(&scratch); /* outer: lives for the whole "step" */

    f64* stages[5]; /* stand-ins for k1..k4 plus a tmp state buffer */
    for (u32 s = 0; s < 5; ++s)
    {
        stages[s] = arena_push_array(&scratch, f64, n);
        for (u64 i = 0; i < n; ++i) stages[s][i] = (f64)(s * 1000 + i); /* sentinel */
    }

    void* first_flux = NULL;
    for (u32 stage = 0; stage < 4; ++stage) /* 4 "RHS evaluations" per step */
    {
        ArenaTemp rhs = arena_begin_temp(&scratch); /* inner: this call only */

        f64* flux = arena_push_array(&scratch, f64, n + 1);
        for (u64 i = 0; i < n + 1; ++i) flux[i] = (f64)i;

        if (stage == 0) first_flux = flux;
        else            ASSERT(flux == first_flux); /* same bytes reused every call */

        arena_end_temp(rhs);
    }

    /* none of the inner begin/end cycles disturbed the outer, stage-persistent buffers */
    b8 outer_intact = true;
    for (u32 s = 0; s < 5; ++s)
        for (u64 i = 0; i < n; ++i)
            if (stages[s][i] != (f64)(s * 1000 + i)) { outer_intact = false; break; }
    ASSERT(outer_intact);

    arena_end_temp(step);
    ASSERT(scratch.pos == base);

    arena_release(&scratch);
}

static void test_clear(void)
{
    SECTION("clear: resets position, preserves reservation");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    arena_push_array(&arena, u8, 256);
    u64 reserved_before = arena.reserved_size;

    arena_clear(&arena);

    ASSERT(arena.pos == 0);
    ASSERT(arena.base != 0);
    ASSERT(arena.reserved_size == reserved_before);

    arena_release(&arena);
}

static void test_decommit_flag(void)
{
    SECTION("flags: Decommit shrinks commit_size on pop/clear, re-commits cleanly after");

    u64 pagesize = os_mem_pagesize();
    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_Decommit);

    arena_push_array(&arena, u8, pagesize * 3);
    ASSERT(arena.commit_size >= pagesize * 3);

    arena_clear(&arena);
    ASSERT(arena.commit_size == 0);

    u8* after = arena_push_array(&arena, u8, pagesize);
    ASSERT(after != 0);
    ASSERT(arena.commit_size >= pagesize);

    arena_release(&arena);
}

static void test_commit_chunked_flag(void)
{
    SECTION("flags: CommitChunked commits in granularity-page chunks, not single pages");

    u64 pagesize    = os_mem_pagesize();
    u32 granularity = 4;
    Arena arena = arena_alloc_ex(MB(1), 0, granularity, ArenaFlags_CommitChunked);

    arena_push_array(&arena, u8, 8); /* tiny push, still pulls in a whole chunk */
    ASSERT(arena.commit_size == pagesize * granularity);

    arena_push_array(&arena, u8, pagesize * granularity - 8 - 1); /* fills chunk 1 minus one byte */
    ASSERT(arena.commit_size == pagesize * granularity); /* still inside chunk 1 */

    arena_push_array(&arena, u8, 2); /* tips over into chunk 2 */
    ASSERT(arena.commit_size == pagesize * granularity * 2);

    arena_release(&arena);
}

static void test_debug_fill_on_clear_flag(void)
{
    SECTION("flags: DebugFillOnClear poisons popped memory with 0xDD");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_DebugFillOnClear);

    u8* data = arena_push_array(&arena, u8, 64);
    memset(data, 0x42, 64);

    u64 mark = arena.pos - 64;
    arena_pop_to(&arena, mark);

    ASSERT(bytes_all_eq(arena.base + mark, 64, 0xDD));

    arena_release(&arena);
}

static void test_strings(void)
{
    SECTION("strings: cstring / cstring_fmt / str8 helpers");

    Arena arena = arena_alloc_ex(MB(1), 0, 1, ArenaFlags_None);

    char* c = arena_push_cstring(&arena, "hello");
    ASSERT(strcmp(c, "hello") == 0);

    char* cf = arena_push_cstring_fmt(&arena, "n=%d", 42);
    ASSERT(strcmp(cf, "n=42") == 0);

    str8 s = arena_push_str8_copy(&arena, STR("world"));
    ASSERT(s.size == 5);
    ASSERT(memcmp(s.data, "world", 5) == 0);

    str8 s2 = arena_push_str8_from_cstring(&arena, "abc");
    ASSERT(s2.size == 3);
    ASSERT(s2.data[3] == 0); /* underlying buffer is still null-terminated, even though size excludes it */

    str8 s3 = arena_push_str8_fmt(&arena, "x=%d", 7);
    ASSERT(s3.size == 3);
    ASSERT(memcmp(s3.data, "x=7", 3) == 0);

    arena_release(&arena);
}

static void test_overflow_guard(void)
{
    SECTION("push: allocation past reserved_size is rejected without corrupting state");

#if AETHER_ENABLE_ASSERTS
    printf("   skipped: ASSERT() traps on overflow in this build (AETHER_ENABLE_ASSERTS=1);\n"
           "            rebuild with NDEBUG defined to exercise the release-mode failure path.\n");
#else
    Arena arena = arena_alloc_ex(KB(4), 0, 1, ArenaFlags_None);

    u64 pos_before = arena.pos;
    void* p = arena_push(&arena, arena.reserved_size + 1, 1, 0);
    ASSERT(p == 0);
    ASSERT(arena.pos == pos_before);

    /* an allocation that exactly fills the remaining reservation still succeeds */
    void* exact = arena_push(&arena, arena.reserved_size, 1, 0);
    ASSERT(exact != 0);
    ASSERT(arena.pos == arena.reserved_size);

    /* and now even one more byte is rejected */
    void* one_more = arena_push(&arena, 1, 1, 0);
    ASSERT(one_more == 0);

    arena_release(&arena);
#endif
}

static void test_release(void)
{
    SECTION("release: zeroes bookkeeping fields");

    Arena arena = arena_alloc(KB(64));
    arena_push_array(&arena, u8, 128);

    arena_release(&arena);

    ASSERT(arena.base == 0);
    ASSERT(arena.reserved_size == 0);
    ASSERT(arena.commit_size == 0);
    ASSERT(arena.pos == 0);
}

static void test_many_small_pushes(void)
{
    SECTION("stress: many incremental pushes across repeated commit growth");

    Arena arena = arena_alloc_ex(MB(4), 0, 1, ArenaFlags_Decommit);

    const u32 n = 50000;
    u32 spot_idx[8] = { 0, 1, 7, 100, 4095, 4096, 25000, n - 1 };
    u32* slots[8] = {0};

    for (u32 i = 0; i < n; ++i)
    {
        u32* slot = arena_push_t(&arena, u32);
        *slot = i;

        for (int s = 0; s < 8; ++s)
            if (spot_idx[s] == i) slots[s] = slot;
    }

    ASSERT(arena.pos == (u64)n * sizeof(u32));

    b8 spots_ok = true;
    for (int s = 0; s < 8; ++s)
        if (*slots[s] != spot_idx[s]) { spots_ok = false; break; }
    ASSERT(spots_ok);

    arena_clear(&arena);
    ASSERT(arena.pos == 0);
    ASSERT(arena.commit_size == 0); /* Decommit flag: clear walks committed pages back down */

    arena_release(&arena);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"array_count",              test_array_count},
    {"alloc_basic",              test_alloc_basic},
    {"alloc_ex_custom",          test_alloc_ex_custom},
    {"push_alignment",           test_push_alignment},
    {"push_array_contiguous",    test_push_array_contiguous},
    {"zero_policy",              test_zero_policy},
    {"pop_to_and_pop",           test_pop_to_and_pop},
    {"temp_basic",               test_temp_basic},
    {"temp_nested",              test_temp_nested},
    {"temp_out_of_order_is_safe", test_temp_out_of_order_is_safe},
    {"temp_rk4_pattern",          test_temp_rk4_pattern},
    {"clear",                    test_clear},
    {"decommit_flag",            test_decommit_flag},
    {"commit_chunked_flag",      test_commit_chunked_flag},
    {"debug_fill_on_clear_flag", test_debug_fill_on_clear_flag},
    {"strings",                  test_strings},
    {"overflow_guard",           test_overflow_guard},
    {"release",                  test_release},
    {"many_small_pushes",        test_many_small_pushes},
};

int main(int argc, char** argv)
{
    if (argc > 1) {
        for (size_t i = 0; i < ARRAY_COUNT(g_cases)+1; ++i)
        {
            if (i == ARRAY_COUNT(g_cases))
            {
                fprintf(stderr, "unknown test case: %s\n", argv[1]);
                return 1; /* signal a fail if no test case found */
            }
            if (strcmp(argv[1], g_cases[i].name) == 0)
            {
                g_cases[i].fn();
                break;
            }
        }
    } else {
        for (size_t i = 0; i < ARRAY_COUNT(g_cases); ++i) g_cases[i].fn();
        section_summary_flush();
    }

      printf("\n%d checks, %d failed\n", g_checks, g_failures);
      return g_failures != 0;
}
