#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* See test_arenas.c for why we define our own ASSERT() via AETHER_NO_ASSERT. */

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

static void test_eq(void)
{
    SECTION("str8_eq: byte-wise equality");

    ASSERT(str8_eq(STR("hello"), STR("hello")));
    ASSERT(!str8_eq(STR("hello"), STR("world")));
    ASSERT(!str8_eq(STR("hello"), STR("hell")));  /* shared prefix, different length */
    ASSERT(str8_eq(STR(""), STR("")));

    str8_view s = STR("same");
    ASSERT(str8_eq(s, s)); /* pointer-identity shortcut */
}

static void test_cmp(void)
{
    SECTION("str8_cmp: three-way ordering");

    ASSERT(str8_cmp(STR("abc"), STR("abc")) == 0);
    ASSERT(str8_cmp(STR("abc"), STR("abd")) < 0);
    ASSERT(str8_cmp(STR("abd"), STR("abc")) > 0);
    ASSERT(str8_cmp(STR("ab"),  STR("abc")) < 0);  /* strict prefix sorts first */
    ASSERT(str8_cmp(STR("abc"), STR("ab"))  > 0);
}

static void test_slice(void)
{
    SECTION("str8_slice: half-open [start, end) substrings");

    str8_view s = STR("hello world");

    str8_view whole = str8_slice(s, 0, s.size);
    ASSERT(whole.size == s.size);
    ASSERT(memcmp(whole.data, "hello world", 11) == 0);

    str8_view word = str8_slice(s, 6, 11);
    ASSERT(word.size == 5);
    ASSERT(memcmp(word.data, "world", 5) == 0);

    str8_view empty = str8_slice(s, 3, 3);
    ASSERT(empty.size == 0);
}

static void test_trim(void)
{
    SECTION("str8_trim: strips leading/trailing whitespace only");

    str8_view a = str8_trim(STR("  hello  "));
    ASSERT(a.size == 5);
    ASSERT(memcmp(a.data, "hello", 5) == 0);

    str8_view b = str8_trim(STR("no_trim"));
    ASSERT(b.size == 7);
    ASSERT(memcmp(b.data, "no_trim", 7) == 0);

    str8_view c = str8_trim(STR("   "));
    ASSERT(c.size == 0);

    str8_view d = str8_trim(STR(""));
    ASSERT(d.size == 0);

    str8_view e = str8_trim(STR("a b\tc\n")); /* embedded whitespace must survive */
    ASSERT(e.size == 5);
    ASSERT(memcmp(e.data, "a b\tc", 5) == 0);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"eq",    test_eq},
    {"cmp",   test_cmp},
    {"slice", test_slice},
    {"trim",  test_trim},
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
