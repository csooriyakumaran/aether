#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* See test_arenas.c for why we define our own ASSERT() via AETHER_NO_ASSERT.
 *
 * Temp files are created in the working directory (the build tree when run
 * under ctest) and removed at the end of each case. */

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

static void test_write_read_roundtrip(void)
{
    SECTION("file_write + file_read: bytes out == bytes in");

    const char* path = "aether_test_roundtrip.tmp";
    str8_view payload = STR("hello, file\nsecond line\0embedded nul survives");

    u64 written = file_write(path, (bytes_view){payload.data, payload.size});
    ASSERT(written == payload.size);

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == payload.size);
    ASSERT(back.data && memcmp(back.data, payload.data, payload.size) == 0);
    arena_release(arena);

    remove(path);
}

static void test_write_truncates(void)
{
    SECTION("file_write: existing file is replaced, not appended");

    const char* path = "aether_test_truncate.tmp";
    str8_view big   = STR("a much longer first payload");
    str8_view small = STR("short");

    ASSERT(file_write(path, (bytes_view){big.data, big.size}) == big.size);
    ASSERT(file_write(path, (bytes_view){small.data, small.size}) == small.size);

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == small.size);
    ASSERT(back.data && memcmp(back.data, small.data, small.size) == 0);
    arena_release(arena);

    remove(path);
}

static void test_write_failure(void)
{
    SECTION("file_write: unwritable path reports 0 bytes");

    str8_view payload = STR("data");
    u64 written = file_write("aether_no_such_dir/x.tmp", (bytes_view){payload.data, payload.size});
    ASSERT(written == 0);
}

static void test_read_missing(void)
{
    SECTION("file_read: missing file yields empty bytes, arena untouched");

    Arena* arena = arena_alloc(KB(4));
    u64 mark = arena->pos;

    bytes b = file_read(arena, "aether_no_such_file.tmp");
    ASSERT(b.data == NULL);
    ASSERT(b.size == 0);
    ASSERT(arena->pos == mark);

    arena_release(arena);
}

static void test_map_roundtrip(void)
{
    SECTION("file_map / file_unmap: read-only view of file contents");

    const char* path = "aether_test_map.tmp";
    str8_view payload = STR("mapped contents");
    ASSERT(file_write(path, (bytes_view){payload.data, payload.size}) == payload.size);

    bytes_view v = file_map(path);
    ASSERT(v.size == payload.size);
    ASSERT(v.data && memcmp(v.data, payload.data, payload.size) == 0);
    file_unmap(v);

    remove(path);
}

static void test_map_missing(void)
{
    SECTION("file_map: missing or empty file yields empty view");

    bytes_view missing = file_map("aether_no_such_file.tmp");
    ASSERT(missing.data == NULL);
    ASSERT(missing.size == 0);

    /* empty file: file_write of zero bytes creates it (and returns 0 --
     * indistinguishable from failure by design, see file_write contract) */
    const char* path = "aether_test_empty.tmp";
    bytes_view none = {0};
    file_write(path, none);

    bytes_view empty = file_map(path);
    ASSERT(empty.data == NULL);
    ASSERT(empty.size == 0);

    remove(path);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"write_read_roundtrip", test_write_read_roundtrip},
    {"write_truncates",      test_write_truncates},
    {"write_failure",        test_write_failure},
    {"read_missing",         test_read_missing},
    {"map_roundtrip",        test_map_roundtrip},
    {"map_missing",          test_map_missing},
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
