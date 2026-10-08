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
    str8 payload = STR("hello, file\nsecond line\0embedded nul survives");

    u64 written = file_write(path, payload.data, payload.size);
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
    str8 big   = STR("a much longer first payload");
    str8 small = STR("short");

    ASSERT(file_write(path, big.data, big.size) == big.size);
    ASSERT(file_write(path, small.data, small.size) == small.size);

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

    str8 payload = STR("data");
    u64 written = file_write("aether_no_such_dir/x.tmp", payload.data, payload.size);
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
    str8 payload = STR("mapped contents");
    ASSERT(file_write(path, payload.data, payload.size) == payload.size);

    view v = file_map(path);
    ASSERT(v.size == payload.size);
    ASSERT(v.data && memcmp(v.data, payload.data, payload.size) == 0);
    file_unmap(v);

    remove(path);
}

static void test_map_missing(void)
{
    SECTION("file_map: missing or empty file yields empty view");

    view missing = file_map("aether_no_such_file.tmp");
    ASSERT(missing.data == NULL);
    ASSERT(missing.size == 0);

    /* empty file: file_write of zero bytes creates it (and returns 0 --
     * indistinguishable from failure by design, see file_write contract) */
    const char* path = "aether_test_empty.tmp";
    file_write(path, NULL, 0);

    view empty = file_map(path);
    ASSERT(empty.data == NULL);
    ASSERT(empty.size == 0);

    remove(path);
}

static void test_stream_write_roundtrip(void)
{
    SECTION("file_stream_open + reserve/commit/flush/close: buffered writes land on disk");

    const char* path = "aether_test_stream_roundtrip.tmp";
    u8 buf[64];
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    bytes dst = file_stream_reserve(&fs, 5);
    ASSERT(dst.data != NULL);
    memcpy(dst.data, "hello", 5);
    file_stream_commit(&fs, 5);

    ASSERT(file_stream_close(&fs));

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == 5);
    ASSERT(back.data && memcmp(back.data, "hello", 5) == 0);
    arena_release(arena);

    remove(path);
}

static void test_stream_fmt_roundtrip(void)
{
    SECTION("file_stream_fmt: formatted text is buffered then flushed correctly");

    const char* path = "aether_test_stream_fmt.tmp";
    u8 buf[64];
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    str8 r1 = file_stream_fmt(&fs, 32, "%s=%d\n", "x", 1);
    ASSERT(str8_eq(r1, STR("x=1\n")));
    str8 r2 = file_stream_fmt(&fs, 32, "%s=%d\n", "y", 2);
    ASSERT(str8_eq(r2, STR("y=2\n")));

    ASSERT(file_stream_close(&fs));

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(str8_eq((str8){back.data, back.size}, STR("x=1\ny=2\n")));
    arena_release(arena);

    remove(path);
}

static void test_stream_auto_flush_on_full(void)
{
    SECTION("file_stream_reserve: a write that won't fit triggers an automatic flush first");

    const char* path = "aether_test_stream_autoflush.tmp";
    u8 buf[8]; /* smaller than the total payload, forces flushes mid-stream */
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    const char* words[] = {"aaaa", "bbbb", "cccc", "dddd"};
    for (int i = 0; i < 4; i++)
    {
        bytes dst = file_stream_reserve(&fs, 4);
        ASSERT(dst.data != NULL);
        memcpy(dst.data, words[i], 4);
        file_stream_commit(&fs, 4);
    }

    ASSERT(file_stream_close(&fs));

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == 16);
    ASSERT(back.data && memcmp(back.data, "aaaabbbbccccdddd", 16) == 0);
    arena_release(arena);

    remove(path);
}

static void test_stream_fmt_truncation(void)
{
    SECTION("file_stream_fmt: output wider than the stream buffer truncates at the buffer, not silently past it");

    const char* path = "aether_test_stream_trunc.tmp";

    /* buffer and cap both 4: formatted text + NUL must fit in 4 bytes exactly,
       so "abcdef" truncates to 3 chars + NUL, same boundary str8_fmt uses */
    u8 buf[4];
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    str8 r = file_stream_fmt(&fs, 4, "%s", "abcdef");
    ASSERT(r.size == 3);
    ASSERT(memcmp(r.data, "abc", 3) == 0);

    ASSERT(file_stream_close(&fs));

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == 3);
    ASSERT(back.data && memcmp(back.data, "abc", 3) == 0);
    arena_release(arena);

    remove(path);
}

static void test_stream_fmt_cap_is_a_ceiling_even_with_slack(void)
{
    SECTION("file_stream_fmt: `cap` bounds the record even when the buffer has far more room free");

    const char* path = "aether_test_stream_cap_ceiling.tmp";
    u8 buf[64]; /* much more room than cap below asks for */
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    /* cap=4 bounds this record to 3 chars + NUL, regardless of the 64 bytes
       actually free in the buffer -- a short cap must not grow opportunistically */
    str8 r = file_stream_fmt(&fs, 4, "%s", "abcdef");
    ASSERT(r.size == 3);
    ASSERT(memcmp(r.data, "abc", 3) == 0);

    ASSERT(file_stream_close(&fs));

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == 3);
    ASSERT(back.data && memcmp(back.data, "abc", 3) == 0);
    arena_release(arena);

    remove(path);
}

static void test_stream_reserve_hands_back_full_remaining_room(void)
{
    SECTION("file_stream_reserve: unlike file_stream_fmt's cap, the raw primitive returns everything free, not just what was asked for");

    const char* path = "aether_test_stream_reserve_slack.tmp";
    u8 buf[64];
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    /* asking for 4 still gets back the whole 64-byte buffer when nothing's
       been committed yet -- callers writing a variable-length blob (not a
       single bounded record) rely on this to use all the space available */
    bytes dst = file_stream_reserve(&fs, 4);
    ASSERT(dst.size == sizeof(buf));

    /* after committing some, the window shrinks to what's actually left,
       still independent of whatever `len` a later reserve call asks for */
    file_stream_commit(&fs, 50);
    bytes dst2 = file_stream_reserve(&fs, 4);
    ASSERT(dst2.size == sizeof(buf) - 50);

    ASSERT(file_stream_close(&fs));
    remove(path);
}

static void test_stream_flush_empty_is_noop(void)
{
    SECTION("file_stream_flush: nothing buffered -> succeeds without touching the file");

    const char* path = "aether_test_stream_flush_empty.tmp";
    u8 buf[16];
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));

    ASSERT(file_stream_flush(&fs)); /* len == 0: no-op success */
    ASSERT(file_stream_close(&fs));

    Arena* arena = arena_alloc(KB(4));
    bytes back = file_read(arena, path);
    ASSERT(back.size == 0); /* file was created (by open) but nothing was ever written */
    arena_release(arena);

    remove(path);
}

static void test_stream_open_failure(void)
{
    SECTION("file_stream_open: bad path leaves the stream invalid, and every call stays a safe no-op");

    u8 buf[16];
    FileStream fs = file_stream_open("aether_no_such_dir/x.tmp", buf, sizeof(buf));
    ASSERT(!file_stream_valid(&fs));

    bytes dst = file_stream_reserve(&fs, 4);
    ASSERT(dst.data != NULL); /* buffer itself is still usable even with no backing file */
    memcpy(dst.data, "data", 4);
    file_stream_commit(&fs, 4);

    ASSERT(file_stream_flush(&fs)); /* no handle: flush has nothing to do, reports success */
    ASSERT(file_stream_close(&fs));
}

static void test_stream_ok_reports_write_failure(void)
{
    SECTION("file_stream_ok: a failed OS write is visible after the fact, not just via flush's own return value");

    const char* path = "aether_test_stream_ok.tmp";
    u8 buf[16];
    FileStream fs = file_stream_open(path, buf, sizeof(buf));
    ASSERT(file_stream_valid(&fs));
    ASSERT(file_stream_ok(&fs)); /* nothing attempted yet */

    bytes dst = file_stream_reserve(&fs, 5);
    memcpy(dst.data, "hello", 5);
    file_stream_commit(&fs, 5);

    /* close the OS handle out from under the stream to force the next write
       to fail, the same as a device going away or a handle being revoked */
    CloseHandle((HANDLE)fs.handle);

    ASSERT(!file_stream_flush(&fs)); /* the write itself failed */
    ASSERT(!file_stream_ok(&fs));    /* ...and file_stream_ok reflects that afterward, independent of flush's return */

    /* the 5 bytes are still accounted for in the buffer -- flush never
       discards unwritten data on failure, even though with the handle gone
       there is no longer anywhere for them to go */
    ASSERT(fs.len == 5);

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
    {"stream_write_roundtrip",      test_stream_write_roundtrip},
    {"stream_fmt_roundtrip",        test_stream_fmt_roundtrip},
    {"stream_auto_flush_on_full",   test_stream_auto_flush_on_full},
    {"stream_fmt_truncation",       test_stream_fmt_truncation},
    {"stream_fmt_cap_is_ceiling",   test_stream_fmt_cap_is_a_ceiling_even_with_slack},
    {"stream_reserve_slack",        test_stream_reserve_hands_back_full_remaining_room},
    {"stream_flush_empty_is_noop",  test_stream_flush_empty_is_noop},
    {"stream_open_failure",         test_stream_open_failure},
    {"stream_ok_write_failure",     test_stream_ok_reports_write_failure},
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
