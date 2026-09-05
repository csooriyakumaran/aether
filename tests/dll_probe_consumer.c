#define AETHER_NO_ASSERT
#define AETHER_DLL
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* Verifies the AETHER_DLL consumer mode: this TU never defines
   AETHER_IMPLEMENTATION, so every AETHER_API function it calls is a
   dllimport resolving into aether_dll_probe (built from dll_probe_lib.c
   with AETHER_BUILD_DLL). Proves the dllexport/dllimport wiring actually
   links and runs, not just compiles -- nothing else in the suite crosses a
   real DLL boundary. */

static int g_checks   = 0;
static int g_failures = 0;

static int section_checks_start    = 0;
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

#define SECTION(name) do {               \
    section_summary_flush();             \
    current_section        = name;       \
    section_checks_start   = g_checks;   \
    section_failures_start = g_failures; \
    printf("-- %s\n", name);             \
} while (0)

static void test_dll_arena_roundtrip(void)
{
    SECTION("AETHER_DLL -- arena alloc/push/pop/release across the DLL boundary");

    Arena* a = arena_alloc(KB(4));
    ASSERT(a != NULL);

    u32* x = (u32*)arena_push(a, sizeof(u32), ARENA_ALIGN(u32), ArenaZero_Force);
    ASSERT(x != NULL);
    ASSERT(*x == 0);
    *x = 42;
    ASSERT(*x == 42);

    u64 pos_after_push = a->pos;
    arena_pop(a, sizeof(u32));
    ASSERT(a->pos < pos_after_push);

    arena_release(a);
}

static void test_dll_strings(void)
{
    SECTION("AETHER_DLL -- str8 ops across the DLL boundary");

    ASSERT(str8_eq(STR("hello"), STR("hello")));
    ASSERT(str8_has_prefix(STR("hello"), STR("he")));

    u64 pos = 0;
    ASSERT(str8_find(STR("hello world"), STR("world"), &pos) && pos == 6);
}

static void test_dll_ring_buffer(void)
{
    SECTION("AETHER_DLL -- ring buffer write/read across the DLL boundary");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    const char* msg = "dll mode";
    ASSERT(ring_buffer_write(&rb, msg, strlen(msg)));

    char buf[32] = {0};
    ASSERT(ring_buffer_read(&rb, buf, strlen(msg)));
    ASSERT(memcmp(buf, msg, strlen(msg)) == 0);

    ring_buffer_release(&rb);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"arena_roundtrip", test_dll_arena_roundtrip},
    {"strings",         test_dll_strings},
    {"ring_buffer",     test_dll_ring_buffer},
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
    }
    section_summary_flush();

    printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures != 0;
}
