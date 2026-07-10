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

static void test_str_macro(void)
{
    SECTION("STR / STR8_ARG / STR8_FMT: literal-to-view and printf round trip");

    str8_view s = STR("hello");
    ASSERT(s.size == 5);
    ASSERT(memcmp(s.data, "hello", 5) == 0);

    str8_view empty = STR("");
    ASSERT(empty.size == 0);

    char buf[32];
    int n = snprintf(buf, sizeof(buf), STR8_FMT, STR8_ARG(s));
    ASSERT(n == 5);
    ASSERT(strcmp(buf, "hello") == 0);
}

static void test_views(void)
{
    SECTION("view_from_str8 / view_from_bytes: borrow without copying");

    Arena* arena = arena_alloc(KB(4));

    str8 owned = str8_push_copy(arena, STR("borrowed"));
    str8_view view = view_from_str8(owned);
    ASSERT(view.data == owned.data);
    ASSERT(view.size == owned.size);

    bytes b = { .data = owned.data, .size = owned.size };
    bytes_view bv = view_from_bytes(b);
    ASSERT(bv.data == b.data);
    ASSERT(bv.size == b.size);

    arena_release(arena);
}

static void test_c_str(void)
{
    SECTION("c_str: str8_view -> null-terminated, arena-allocated C string");

    Arena* arena = arena_alloc(KB(4));

    char* h = c_str(arena, STR("hello"));
    ASSERT(strcmp(h, "hello") == 0);

    str8 copy = str8_push_copy(arena, STR("world"));
    char* w = c_str(arena, view_from_str8(copy));
    ASSERT(strcmp(w, "world") == 0);

    char* e = c_str(arena, STR(""));
    ASSERT(strcmp(e, "") == 0);

    char* z = c_str(arena, (str8_view){0}); /* NULL data, zero size */
    ASSERT(strcmp(z, "") == 0);

    arena_release(arena);
}

static void test_split(void)
{
    SECTION("str8_split / str8_list_push: tokenizing with and without empty runs");

    Arena* arena = arena_alloc(KB(4));

    /* default: empty runs between separators are kept */
    Str8List all = str8_split(arena, STR("a,,b"), STR(","), Str8SplitFlags_None);
    ASSERT(all.count == 3);
    if (all.count == 3) {
        Str8Node* n = all.first;
        ASSERT(str8_eq(n->v, STR("a"))); n = n->next;
        ASSERT(str8_eq(n->v, STR("")));  n = n->next;
        ASSERT(str8_eq(n->v, STR("b")));
    }

    /* Str8SplitFlags_SkipEmpty: empty runs are dropped */
    Str8List skip = str8_split(arena, STR("a,,b"), STR(","), Str8SplitFlags_SkipEmpty);
    ASSERT(skip.count == 2);
    if (skip.count == 2) {
        ASSERT(str8_eq(skip.first->v, STR("a")));
        ASSERT(str8_eq(skip.first->next->v, STR("b")));
    }

    /* no separator present: whole string comes back as a single node */
    Str8List none = str8_split(arena, STR("abc"), STR(","), Str8SplitFlags_None);
    ASSERT(none.count == 1);
    ASSERT(str8_eq(none.first->v, STR("abc")));

    arena_release(arena);
}

static void test_skip_drop(void)
{
    SECTION("str8_skip / str8_drop: views without the first/last n bytes");

    str8_view s = STR("hello");

    ASSERT(str8_eq(str8_skip(s, 0), s));
    ASSERT(str8_eq(str8_skip(s, 2), STR("llo")));
    ASSERT(str8_skip(s, s.size).size == 0);

    ASSERT(str8_eq(str8_drop(s, 0), s));
    ASSERT(str8_eq(str8_drop(s, 2), STR("hel")));
    ASSERT(str8_drop(s, s.size).size == 0);
}

static void test_trim_lr(void)
{
    SECTION("str8_trim_left / str8_trim_right: one-sided whitespace strip");

    ASSERT(str8_eq(str8_trim_left(STR("  a ")),  STR("a ")));
    ASSERT(str8_eq(str8_trim_right(STR("  a ")), STR("  a")));
    ASSERT(str8_trim_left(STR("   ")).size == 0);
    ASSERT(str8_trim_right(STR("   ")).size == 0);
    ASSERT(str8_eq(str8_trim_left(STR("a")),  STR("a")));
    ASSERT(str8_eq(str8_trim_right(STR("a")), STR("a")));
}

static void test_eq_nocase(void)
{
    SECTION("str8_eq_nocase: ASCII case-insensitive equality");

    ASSERT(str8_eq_nocase(STR("Hello"), STR("hELLO")));
    ASSERT(str8_eq_nocase(STR("a1! z"), STR("A1! Z"))); /* non-alpha bytes must compare exactly */
    ASSERT(!str8_eq_nocase(STR("hello"), STR("world")));
    ASSERT(!str8_eq_nocase(STR("hello"), STR("hell")));
    ASSERT(!str8_eq_nocase(STR("a{"), STR("a[")));       /* '{' vs '[' differ by 32 but are not letters */
    ASSERT(str8_eq_nocase(STR(""), STR("")));
}

static void test_prefix_suffix(void)
{
    SECTION("str8_has_prefix / str8_has_suffix");

    ASSERT(str8_has_prefix(STR("hello"), STR("he")));
    ASSERT(str8_has_prefix(STR("hello"), STR("hello")));
    ASSERT(!str8_has_prefix(STR("hello"), STR("el")));
    ASSERT(!str8_has_prefix(STR("he"), STR("hello"))); /* prefix longer than s */
    ASSERT(!str8_has_prefix(STR("hello"), STR("")));   /* current semantics: empty prefix -> false */

    ASSERT(str8_has_suffix(STR("hello"), STR("lo")));
    ASSERT(str8_has_suffix(STR("hello"), STR("hello")));
    ASSERT(!str8_has_suffix(STR("hello"), STR("ll")));
    ASSERT(!str8_has_suffix(STR("lo"), STR("hello")));
    ASSERT(!str8_has_suffix(STR("hello"), STR("")));   /* current semantics: empty suffix -> false */
}

static void test_find(void)
{
    SECTION("str8_find / str8_find_last / str8_find_char");

    u64 pos = 0;

    ASSERT(str8_find(STR("hello world"), STR("world"), &pos) && pos == 6);
    ASSERT(str8_find(STR("hello"), STR("he"), &pos) && pos == 0);
    ASSERT(str8_find(STR("aXaX"), STR("X"), &pos) && pos == 1); /* first occurrence */
    ASSERT(!str8_find(STR("hello"), STR("z"), &pos));
    ASSERT(!str8_find(STR("he"), STR("hello"), &pos));          /* needle longer than s */
    ASSERT(str8_find(STR("hello"), STR(""), &pos) && pos == 0); /* empty needle matches at 0 */

    ASSERT(str8_find_last(STR("abcabc"), STR("abc"), &pos) && pos == 3);
    ASSERT(str8_find_last(STR("abc"), STR("abc"), &pos) && pos == 0);
    ASSERT(!str8_find_last(STR("hello"), STR("z"), &pos));
    ASSERT(str8_find_last(STR("hello"), STR(""), &pos) && pos == 5); /* empty needle -> s.size */
    /* scan is non-overlapping: in "aaa" the matches are at 0 and (skipping 2) nowhere else */
    ASSERT(str8_find_last(STR("aaa"), STR("aa"), &pos) && pos == 0);

    ASSERT(str8_find_char(STR("hello"), 'l', &pos) && pos == 2); /* first occurrence */
    ASSERT(!str8_find_char(STR("hello"), 'z', &pos));
    ASSERT(!str8_find_char(STR(""), 'a', &pos));
}

static void test_cut(void)
{
    SECTION("str8_cut: split at first separator into before/after");

    str8_view before, after;

    ASSERT(str8_cut(STR("key=value"), STR("="), &before, &after));
    ASSERT(str8_eq(before, STR("key")));
    ASSERT(str8_eq(after,  STR("value")));

    ASSERT(str8_cut(STR("a::b::c"), STR("::"), &before, &after)); /* multi-byte sep, first match wins */
    ASSERT(str8_eq(before, STR("a")));
    ASSERT(str8_eq(after,  STR("b::c")));

    ASSERT(str8_cut(STR("=x"), STR("="), &before, &after)); /* sep at start: empty before */
    ASSERT(before.size == 0);
    ASSERT(str8_eq(after, STR("x")));

    ASSERT(!str8_cut(STR("no separator"), STR("="), &before, &after)); /* miss: before = whole input */
    ASSERT(str8_eq(before, STR("no separator")));
    ASSERT(after.size == 0);

    ASSERT(str8_cut(STR("a=b"), STR("="), NULL, NULL)); /* NULL outs are allowed */
}

static void test_concat(void)
{
    SECTION("str8_concat: a + b, nul-terminated on the arena");

    Arena* arena = arena_alloc(KB(4));

    str8 ab = str8_concat(arena, STR("foo"), STR("bar"));
    ASSERT(str8_eq(view_from_str8(ab), STR("foobar")));
    ASSERT(ab.data[ab.size] == '\0');

    str8 ea = str8_concat(arena, STR(""), STR("x"));
    ASSERT(str8_eq(view_from_str8(ea), STR("x")));

    str8 copy = str8_push_copy(arena, STR("hi"));
    ASSERT(copy.data[copy.size] == '\0'); /* push_copy follows the nul-termination convention */

    arena_release(arena);
}

static void test_join(void)
{
    SECTION("str8_list_push_fmt / str8_join / str8_list_to_array");

    Arena* arena = arena_alloc(KB(4));

    Str8List list = {0};
    str8_list_push(arena, &list, STR("a"));
    str8_list_push_fmt(arena, &list, "%d", 42);
    str8_list_push(arena, &list, STR("c"));
    ASSERT(list.count == 3);
    ASSERT(list.total_len == 4); /* "a" + "42" + "c" */

    str8 joined = str8_join(arena, &list, STR(", "));
    ASSERT(str8_eq(view_from_str8(joined), STR("a, 42, c")));
    ASSERT(joined.data[joined.size] == '\0');

    Str8List single = {0};
    str8_list_push(arena, &single, STR("only"));
    ASSERT(str8_eq(view_from_str8(str8_join(arena, &single, STR("|"))), STR("only")));

    Str8List empty = {0};
    str8 none = str8_join(arena, &empty, STR("|"));
    ASSERT(none.size == 0);

    Str8Array arr = str8_list_to_array(arena, &list);
    ASSERT(arr.count == 3);
    if (arr.count == 3) {
        ASSERT(str8_eq(arr.items[0], STR("a")));
        ASSERT(str8_eq(arr.items[1], STR("42")));
        ASSERT(str8_eq(arr.items[2], STR("c")));
    }

    arena_release(arena);
}

static void test_case_convert(void)
{
    SECTION("str8_to_upper / str8_to_lower: ASCII letters only, rest untouched");

    Arena* arena = arena_alloc(KB(4));

    str8 up = str8_to_upper(arena, STR("Hello, World! 123"));
    ASSERT(str8_eq(view_from_str8(up), STR("HELLO, WORLD! 123")));
    ASSERT(up.data[up.size] == '\0');

    str8 lo = str8_to_lower(arena, STR("Hello, World! 123"));
    ASSERT(str8_eq(view_from_str8(lo), STR("hello, world! 123")));
    ASSERT(lo.data[lo.size] == '\0');

    ASSERT(str8_to_upper(arena, STR("")).size == 0);
    ASSERT(str8_to_lower(arena, STR("")).size == 0);

    arena_release(arena);
}

static void test_replace(void)
{
    SECTION("str8_replace: replace all non-overlapping occurrences");

    Arena* arena = arena_alloc(KB(4));

    str8 grow = str8_replace(arena, STR("aXbXc"), STR("X"), STR("YY"));
    ASSERT(str8_eq(view_from_str8(grow), STR("aYYbYYc")));
    ASSERT(grow.data[grow.size] == '\0');

    str8 shrink = str8_replace(arena, STR("aXXbXXc"), STR("XX"), STR("_")); /* target shorter than old */
    ASSERT(str8_eq(view_from_str8(shrink), STR("a_b_c")));

    str8 del = str8_replace(arena, STR("aXbXc"), STR("X"), STR("")); /* empty target deletes */
    ASSERT(str8_eq(view_from_str8(del), STR("abc")));

    str8 miss = str8_replace(arena, STR("abc"), STR("X"), STR("Y")); /* old not present: plain copy */
    ASSERT(str8_eq(view_from_str8(miss), STR("abc")));

    str8 noop = str8_replace(arena, STR("abc"), STR(""), STR("Y")); /* empty old: plain copy */
    ASSERT(str8_eq(view_from_str8(noop), STR("abc")));

    str8 nonoverlap = str8_replace(arena, STR("aaa"), STR("aa"), STR("b"));
    ASSERT(str8_eq(view_from_str8(nonoverlap), STR("ba")));

    arena_release(arena);
}

static void test_parse(void)
{
    SECTION("str8_to_u64 / str8_to_i64 / str8_to_f64");

    u64 u = 0;
    ASSERT(str8_to_u64(STR("0"), &u)   && u == 0);
    ASSERT(str8_to_u64(STR("42"), &u)  && u == 42);
    ASSERT(str8_to_u64(STR("+42"), &u) && u == 42);
    ASSERT(str8_to_u64(STR("18446744073709551615"), &u) && u == U64_MAX);
    ASSERT(!str8_to_u64(STR("18446744073709551616"), &u)); /* U64_MAX + 1 overflows */
    ASSERT(!str8_to_u64(STR("-1"), &u));
    ASSERT(!str8_to_u64(STR(""), &u));
    ASSERT(!str8_to_u64(STR("+"), &u));   /* sign alone has no digits */
    ASSERT(!str8_to_u64(STR("12a"), &u));
    ASSERT(!str8_to_u64(STR(" 12"), &u)); /* no whitespace tolerance */

    i64 i = 0;
    ASSERT(str8_to_i64(STR("42"), &i)  && i == 42);
    ASSERT(str8_to_i64(STR("-42"), &i) && i == -42);
    ASSERT(str8_to_i64(STR("+7"), &i)  && i == 7);
    ASSERT(str8_to_i64(STR("9223372036854775807"), &i)  && i == I64_MAX);
    ASSERT(str8_to_i64(STR("-9223372036854775808"), &i) && i == I64_MIN);
    ASSERT(!str8_to_i64(STR("9223372036854775808"), &i));  /* I64_MAX + 1 */
    ASSERT(!str8_to_i64(STR("-9223372036854775809"), &i)); /* I64_MIN - 1 */
    ASSERT(!str8_to_i64(STR("-"), &i));
    ASSERT(!str8_to_i64(STR(""), &i));
    ASSERT(!str8_to_i64(STR("1.5"), &i));

    f64 f = 0;
    ASSERT(str8_to_f64(STR("1.5"), &f)    && f == 1.5);
    ASSERT(str8_to_f64(STR("-2.5e3"), &f) && f == -2500.0);
    ASSERT(str8_to_f64(STR("0"), &f)      && f == 0.0);
    ASSERT(!str8_to_f64(STR(""), &f));
    ASSERT(!str8_to_f64(STR("abc"), &f));
    ASSERT(!str8_to_f64(STR("1.5x"), &f)); /* trailing junk rejected */
}

static void test_view_sources(void)
{
    SECTION("view_from_c_str / view_from_raw");

    str8_view s = view_from_c_str("abc");
    ASSERT(s.size == 3);
    ASSERT(memcmp(s.data, "abc", 3) == 0);

    ASSERT(view_from_c_str(NULL).size == 0);
    ASSERT(view_from_c_str("").size == 0);

    const char raw[] = {'x', 'y', 'z'};
    str8_view r = view_from_raw(raw, 3);
    ASSERT(r.size == 3);
    ASSERT(r.data == (const u8*)raw); /* borrow, not copy */
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"eq",    test_eq},
    {"cmp",   test_cmp},
    {"slice",     test_slice},
    {"trim",      test_trim},
    {"str_macro", test_str_macro},
    {"views",     test_views},
    {"c_str",     test_c_str},
    {"split",     test_split},
    {"skip_drop",     test_skip_drop},
    {"trim_lr",       test_trim_lr},
    {"eq_nocase",     test_eq_nocase},
    {"prefix_suffix", test_prefix_suffix},
    {"find",          test_find},
    {"cut",           test_cut},
    {"concat",        test_concat},
    {"join",          test_join},
    {"case_convert",  test_case_convert},
    {"replace",       test_replace},
    {"parse",         test_parse},
    {"view_sources",  test_view_sources},
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
