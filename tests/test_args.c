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

typedef struct BasicArgs
{
    b8   help;
    str8 input;
    str8 name;
} BasicArgs;

static void test_flag_str_and_positional(void)
{
    SECTION("args_parse: Flag, Str, and positional kinds");

    BasicArgs args = {0};
    ArgSpec opts[] = {
        { STR("INPUT"),   {0},       ArgKind_Str,  &args.input, {0},          STR("input file"), true },
        { STR("--help"),  STR("-h"), ArgKind_Flag, &args.help,  {0},          STR("show help") },
        { STR("--name"),  {0},       ArgKind_Str,  &args.name,  STR("VALUE"), STR("a name") },
    };

    char* argv[] = {"prog", "in.csv", "--help", "--name", "value"};
    str8 err = {0};
    b8 ok = args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, &err);

    ASSERT(ok);
    ASSERT(args.help == true);
    ASSERT(str8_eq(args.input, STR("in.csv")));
    ASSERT(str8_eq(args.name, STR("value")));
    ASSERT(err.size == 0); /* nothing to report on success */
}

static void test_alias_and_inline_value(void)
{
    SECTION("args_parse: alias matching, `--name=value` vs `--name value`");

    BasicArgs a1 = {0}, a2 = {0};
    ArgSpec opts1[] = { { STR("--name"), STR("-n"), ArgKind_Str, &a1.name, STR("VALUE"), STR("a name") } };
    ArgSpec opts2[] = { { STR("--name"), STR("-n"), ArgKind_Str, &a2.name, STR("VALUE"), STR("a name") } };

    char* argv_alias[]  = {"prog", "-n", "aliased"};
    char* argv_inline[] = {"prog", "--name=inlined"};

    ASSERT(args_parse((int)ARRAY_COUNT(argv_alias),  argv_alias,  opts1, ARRAY_COUNT(opts1), NULL, NULL));
    ASSERT(str8_eq(a1.name, STR("aliased")));

    ASSERT(args_parse((int)ARRAY_COUNT(argv_inline), argv_inline, opts2, ARRAY_COUNT(opts2), NULL, NULL));
    ASSERT(str8_eq(a2.name, STR("inlined")));
}

static void test_flag_rejects_inline_value(void)
{
    SECTION("args_parse: a Flag-kind option with an inline value (--verbose=false) is a parse error, not a silently-true flag");

    /* before this was fixed, has_inline/inline_val were computed but only consulted
       when spec->kind != ArgKind_Flag -- a Flag spec fell straight into
       `case ArgKind_Flag: *(b8*)spec->out = true;`, discarding the attached value
       and setting the flag true regardless of what came after the "=" */
    BasicArgs args = {0};
    ArgSpec opts[] = { { STR("--verbose"), {0}, ArgKind_Flag, &args.help, {0}, STR("") } };

    char* argv_false[] = {"prog", "--verbose=false"};
    Arena* arena = arena_alloc(KB(4));
    str8 err = {0};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_false), argv_false, opts, ARRAY_COUNT(opts), arena, &err));
    ASSERT(err.size > 0);
    ASSERT(args.help == false);

    args = (BasicArgs){0};
    char* argv_anything[] = {"prog", "--verbose=anything"};
    err = (str8){0};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_anything), argv_anything, opts, ARRAY_COUNT(opts), arena, &err));
    ASSERT(args.help == false);

    /* "--verbose=" (truly empty after the "=") matches the project's existing "empty
       inline value is the same as no inline value at all" convention -- it's not a
       distinct value, so it's not a has_inline case, and the flag is just set true
       the same as a bare "--verbose" would */
    args = (BasicArgs){0};
    char* argv_empty[] = {"prog", "--verbose="};
    ASSERT(args_parse((int)ARRAY_COUNT(argv_empty), argv_empty, opts, ARRAY_COUNT(opts), NULL, NULL));
    ASSERT(args.help == true);

    /* a bare "--verbose", with no "=" at all, is unaffected -- still sets the flag */
    args = (BasicArgs){0};
    char* argv_bare[] = {"prog", "--verbose"};
    ASSERT(args_parse((int)ARRAY_COUNT(argv_bare), argv_bare, opts, ARRAY_COUNT(opts), NULL, NULL));
    ASSERT(args.help == true);

    arena_release(arena);
}

typedef struct NumArgs
{
    u8  u8v;  u16 u16v; u32 u32v; u64 u64v;
    i8  i8v;  i16 i16v; i32 i32v; i64 i64v;
    f64 f64v;
} NumArgs;

static ArgSpec* num_opts(NumArgs* a, u64* out_count)
{
    static ArgSpec opts[9];
    opts[0] = (ArgSpec){ STR("--u8"),  {0}, ArgKind_U8,  &a->u8v,  STR("V"), STR("") };
    opts[1] = (ArgSpec){ STR("--u16"), {0}, ArgKind_U16, &a->u16v, STR("V"), STR("") };
    opts[2] = (ArgSpec){ STR("--u32"), {0}, ArgKind_U32, &a->u32v, STR("V"), STR("") };
    opts[3] = (ArgSpec){ STR("--u64"), {0}, ArgKind_U64, &a->u64v, STR("V"), STR("") };
    opts[4] = (ArgSpec){ STR("--i8"),  {0}, ArgKind_I8,  &a->i8v,  STR("V"), STR("") };
    opts[5] = (ArgSpec){ STR("--i16"), {0}, ArgKind_I16, &a->i16v, STR("V"), STR("") };
    opts[6] = (ArgSpec){ STR("--i32"), {0}, ArgKind_I32, &a->i32v, STR("V"), STR("") };
    opts[7] = (ArgSpec){ STR("--i64"), {0}, ArgKind_I64, &a->i64v, STR("V"), STR("") };
    opts[8] = (ArgSpec){ STR("--f64"), {0}, ArgKind_F64, &a->f64v, STR("V"), STR("") };
    *out_count = ARRAY_COUNT(opts);
    return opts;
}

static void test_numeric_kinds_roundtrip(void)
{
    SECTION("args_parse: every numeric ArgKind stores its value correctly");

    NumArgs a = {0};
    u64 count;
    ArgSpec* opts = num_opts(&a, &count);

    char* argv[] = {
        "prog",
        "--u8", "200", "--u16", "60000", "--u32", "4000000000", "--u64", "9999999999",
        "--i8", "-100", "--i16", "-30000", "--i32", "-2000000000", "--i64", "-9999999999",
        "--f64", "1.5"
    };

    ASSERT(args_parse((int)ARRAY_COUNT(argv), argv, opts, count, NULL, NULL));
    ASSERT(a.u8v  == 200);
    ASSERT(a.u16v == 60000);
    ASSERT(a.u32v == 4000000000u);
    ASSERT(a.u64v == 9999999999ull);
    ASSERT(a.i8v  == -100);
    ASSERT(a.i16v == -30000);
    ASSERT(a.i32v == -2000000000);
    ASSERT(a.i64v == -9999999999ll);
    ASSERT(a.f64v == 1.5);
}

static void test_numeric_zero_is_a_valid_value(void)
{
    SECTION("args_parse: 0 is a legitimate value for unsigned and signed kinds (regression: must not be rejected)");

    NumArgs a = {0};
    a.u8v = 7; a.i32v = 7; /* pre-set to non-zero so a no-op would be visible */
    u64 count;
    ArgSpec* opts = num_opts(&a, &count);

    char* argv[] = {"prog", "--u8", "0", "--i32", "0"};

    ASSERT(args_parse((int)ARRAY_COUNT(argv), argv, opts, count, NULL, NULL));
    ASSERT(a.u8v  == 0);
    ASSERT(a.i32v == 0);
}

static void test_numeric_kind_rejects_bad_value(void)
{
    SECTION("args_parse: non-numeric or out-of-range values fail, with detail when an arena is supplied");

    NumArgs a = {0};
    u64 count;
    ArgSpec* opts1 = num_opts(&a, &count);
    ArgSpec* opts2 = num_opts(&a, &count);

    Arena* arena = arena_alloc(KB(4));

    char* argv_nan[]      = {"prog", "--u8", "notanumber"};
    str8 err1 = {0};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_nan), argv_nan, opts1, count, arena, &err1));
    ASSERT(err1.size > 0);

    char* argv_overflow[] = {"prog", "--u8", "256"}; /* one past U8_MAX */
    str8 err2 = {0};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_overflow), argv_overflow, opts2, count, arena, &err2));
    ASSERT(err2.size > 0);

    arena_release(arena);
}

static void test_unrecognized_option(void)
{
    SECTION("args_parse: unrecognized option fails; NULL arena/out_err stays safe");

    BasicArgs args = {0};
    ArgSpec opts[] = { { STR("--name"), {0}, ArgKind_Str, &args.name, STR("V"), STR("") } };

    char* argv[] = {"prog", "--nope", "x"};

    ASSERT(!args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, NULL));

    Arena* arena = arena_alloc(KB(4));
    str8 err = {0};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), arena, &err));
    ASSERT(err.size > 0);
    arena_release(arena);
}

static void test_missing_value_for_option(void)
{
    SECTION("args_parse: a value-taking option at the end of argv with nothing after it fails");

    BasicArgs args = {0};
    ArgSpec opts[] = { { STR("--name"), {0}, ArgKind_Str, &args.name, STR("V"), STR("") } };

    char* argv[] = {"prog", "--name"};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, NULL));
}

static void test_empty_inline_value_falls_through_to_next_token(void)
{
    SECTION("args_parse: an empty \"--name=\" is treated the same as no \"=\" at all, not a silent empty value");

    /* "--name=" with truly nothing after it is a missing value, same as "--name" alone --
       not a silently-accepted empty string for Str kind */
    BasicArgs args = {0};
    ArgSpec dangling[] = { { STR("--name"), {0}, ArgKind_Str, &args.name, STR("V"), STR("") } };
    char* argv_dangling[] = {"prog", "--name="};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_dangling), argv_dangling, dangling, ARRAY_COUNT(dangling), NULL, NULL));

    /* the realistic shell-split typo: an unquoted "--Mach= 1.4" arrives as two argv
       elements ("--Mach=", "1.4"), not one -- this must not hard-fail the whole parse */
    f64 mach = 0;
    ArgSpec numeric[] = { { STR("--Mach"), {0}, ArgKind_F64, &mach, STR("V"), STR("") } };
    char* argv_numeric[] = {"prog", "--Mach=", "1.4"};
    ASSERT(args_parse((int)ARRAY_COUNT(argv_numeric), argv_numeric, numeric, ARRAY_COUNT(numeric), NULL, NULL));
    ASSERT(mach == 1.4);

    /* same typo against a Str option is the dangerous case: before this was fixed, the
       empty inline value was silently accepted and "value" was misread as an unrelated
       positional instead of ever reaching --name */
    BasicArgs args2 = {0};
    ArgSpec withpos[] = {
        { STR("INPUT"),  {0}, ArgKind_Str, &args2.input, {0},      STR(""), true },
        { STR("--name"), {0}, ArgKind_Str, &args2.name,  STR("V"), STR("") },
    };
    char* argv_str[] = {"prog", "--name=", "value"};
    ASSERT(args_parse((int)ARRAY_COUNT(argv_str), argv_str, withpos, ARRAY_COUNT(withpos), NULL, NULL));
    ASSERT(str8_eq(args2.name, STR("value")));
    ASSERT(args2.input.size == 0); /* "value" went to --name, not to the INPUT positional */
}

static void test_unexpected_positional(void)
{
    SECTION("args_parse: more positional tokens than declared positional specs fails");

    BasicArgs args = {0};
    ArgSpec opts[] = { { STR("INPUT"), {0}, ArgKind_Str, &args.input, {0}, STR(""), true } };

    char* argv[] = {"prog", "a.csv", "b.csv"}; /* only one positional declared */
    ASSERT(!args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, NULL));
}

static void test_required_positional_missing(void)
{
    SECTION("args_parse: a required positional that's never supplied fails, with detail");

    BasicArgs args = {0};
    ArgSpec opts[] = {
        { STR("INPUT"), {0}, ArgKind_Str, &args.input, {0}, STR(""), true, true }, /* positional, required */
    };

    char* argv_missing[] = {"prog"};
    Arena* arena = arena_alloc(KB(4));
    str8 err = {0};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_missing), argv_missing, opts, ARRAY_COUNT(opts), arena, &err));
    ASSERT(err.size > 0);

    args = (BasicArgs){0};
    char* argv_present[] = {"prog", "in.csv"};
    ASSERT(args_parse((int)ARRAY_COUNT(argv_present), argv_present, opts, ARRAY_COUNT(opts), arena, &err));
    ASSERT(str8_eq(args.input, STR("in.csv")));

    arena_release(arena);
}

static void test_required_keyword_missing(void)
{
    SECTION("args_parse: a required keyword option that's never supplied fails");

    BasicArgs args = {0};
    ArgSpec opts[] = {
        { STR("--name"), {0}, ArgKind_Str, &args.name, STR("V"), STR(""), false, true }, /* keyword, required */
    };

    char* argv[] = {"prog"};
    ASSERT(!args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, NULL));
}

static void test_was_set_tracking(void)
{
    SECTION("args_parse: was_set distinguishes an explicit value from a pre-filled default");

    BasicArgs args = {0};
    args.name = STR("default"); /* caller-supplied default, not from the user */

    ArgSpec opts[] = {
        { STR("--help"), STR("-h"), ArgKind_Flag, &args.help, {0},          STR("") },
        { STR("--name"), {0},       ArgKind_Str,  &args.name, STR("VALUE"), STR("") },
    };

    char* argv[] = {"prog", "--help"}; /* --name never passed */
    ASSERT(args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, NULL));

    ASSERT(opts[0].was_set == true);   /* --help was passed */
    ASSERT(opts[1].was_set == false);  /* --name was not; args.name still holds the caller's default */
    ASSERT(str8_eq(args.name, STR("default")));
}

static void test_was_set_reset_across_calls(void)
{
    SECTION("args_parse: was_set is reset at the top of every call, so a reused ArgSpec table doesn't inherit stale state from a prior parse");

    /* before this was fixed, was_set was written true but never reset anywhere in
       args_parse -- a spec satisfied in one call stayed was_set == true forever, so
       a second parse reusing the same table could omit a required field and the
       required check would still silently pass */
    BasicArgs args = {0};
    ArgSpec opts[] = {
        { STR("--name"), {0}, ArgKind_Str, &args.name, STR("V"), STR(""), false, true }, /* keyword, required */
    };

    char* argv_present[] = {"prog", "--name", "first"};
    ASSERT(args_parse((int)ARRAY_COUNT(argv_present), argv_present, opts, ARRAY_COUNT(opts), NULL, NULL));
    ASSERT(opts[0].was_set == true);

    char* argv_missing[] = {"prog"}; /* --name omitted this time, same spec table reused */
    ASSERT(!args_parse((int)ARRAY_COUNT(argv_missing), argv_missing, opts, ARRAY_COUNT(opts), NULL, NULL));
    ASSERT(opts[0].was_set == false);
}

static void test_err_out_cleared_on_success(void)
{
    SECTION("args_parse: *out_err is reset to empty on a successful parse, even if it held something before the call");

    BasicArgs args = {0};
    ArgSpec opts[] = { { STR("--help"), STR("-h"), ArgKind_Flag, &args.help, {0}, STR("") } };

    char* argv[] = {"prog", "--help"};
    str8 err = STR("stale leftover from a previous call");

    ASSERT(args_parse((int)ARRAY_COUNT(argv), argv, opts, ARRAY_COUNT(opts), NULL, &err));
    ASSERT(err.size == 0);
}

static void test_has_flag_prescan(void)
{
    SECTION("args_has_flag: raw argv presence check, independent of any ArgSpec table");

    char* argv_name[]  = {"prog", "in.csv", "--help"};
    char* argv_alias[] = {"prog", "-h", "in.csv"};
    char* argv_none[]  = {"prog", "in.csv", "--other"};

    ASSERT(args_has_flag((int)ARRAY_COUNT(argv_name),  argv_name,  STR("--help"), STR("-h")));
    ASSERT(args_has_flag((int)ARRAY_COUNT(argv_alias), argv_alias, STR("--help"), STR("-h")));
    ASSERT(!args_has_flag((int)ARRAY_COUNT(argv_none), argv_none,  STR("--help"), STR("-h")));

    /* still finds it even when something else on the line would fail a real args_parse */
    char* argv_with_bogus[] = {"prog", "--bogus-flag", "--help"};
    ASSERT(args_has_flag((int)ARRAY_COUNT(argv_with_bogus), argv_with_bogus, STR("--help"), STR("-h")));

    /* no alias to check: {0} is safe, never matches */
    ASSERT(!args_has_flag((int)ARRAY_COUNT(argv_name), argv_name, STR("--version"), (str8){0}));
}

static void test_has_option_prescan(void)
{
    SECTION("args_has_option: raw argv presence + value check, independent of any ArgSpec table");

    str8 val = {0};

    char* argv_inline[] = {"prog", "in.csv", "--port=8080"};
    ASSERT(args_has_option((int)ARRAY_COUNT(argv_inline), argv_inline, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("8080")));

    val = (str8){0};
    char* argv_separate[] = {"prog", "--port", "9090", "in.csv"};
    ASSERT(args_has_option((int)ARRAY_COUNT(argv_separate), argv_separate, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("9090")));

    val = (str8){0};
    char* argv_alias[] = {"prog", "-p", "1234"};
    ASSERT(args_has_option((int)ARRAY_COUNT(argv_alias), argv_alias, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("1234")));

    /* found but with nothing to read as a value -- treated as not found, same as args_parse's
       own "requires a value" failure, just without a diagnostic at this no-ceremony tier */
    char* argv_dangling[] = {"prog", "--port"};
    val = STR("untouched");
    ASSERT(!args_has_option((int)ARRAY_COUNT(argv_dangling), argv_dangling, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("untouched"))); /* out_val is only ever written on a true return */

    char* argv_missing[] = {"prog", "in.csv", "--other"};
    ASSERT(!args_has_option((int)ARRAY_COUNT(argv_missing), argv_missing, STR("--port"), STR("-p"), NULL));

    /* "--port=" with truly nothing after it anywhere -- just as unusable as the
       dangling "--port" above */
    char* argv_empty_inline[] = {"prog", "--port="};
    val = STR("untouched");
    ASSERT(!args_has_option((int)ARRAY_COUNT(argv_empty_inline), argv_empty_inline, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("untouched")));

    /* "--port=" immediately followed by a separate token -- this is what a real shell
       actually produces from an unquoted "--port= 8080" (the space ends the argv
       element right there), so an empty inline value falls through to the next token
       exactly like "no '=' at all" would, instead of giving up */
    char* argv_empty_inline_then_value[] = {"prog", "--port=", "8080"};
    val = (str8){0};
    ASSERT(args_has_option((int)ARRAY_COUNT(argv_empty_inline_then_value), argv_empty_inline_then_value, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("8080")));

    /* a single token that is only whitespace after "=" is still unusable -- falling
       through to the next token only happens when the inline value is truly empty,
       not merely blank, so this does NOT pick up anything further down argv */
    char* argv_whitespace_inline[] = {"prog", "--port= "};
    val = STR("untouched");
    ASSERT(!args_has_option((int)ARRAY_COUNT(argv_whitespace_inline), argv_whitespace_inline, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR("untouched")));

    /* the embedded-space form only arises from explicit quoting ("--port= 8080" as one
       argv element), unlike the realistic two-token typo above. args_has_option still
       reports it found, but hands back the raw, untrimmed text, so the caller's own
       str8_to_* conversion (which already rejects leading whitespace everywhere in this
       header) is what ultimately rejects it, not args_has_option itself */
    char* argv_space_before_value[] = {"prog", "--port= 8080"};
    val = (str8){0};
    ASSERT(args_has_option((int)ARRAY_COUNT(argv_space_before_value), argv_space_before_value, STR("--port"), STR("-p"), &val));
    ASSERT(str8_eq(val, STR(" 8080")));
    u16 space_port = 999;
    ASSERT(!str8_to_u16(val, &space_port)); /* rejected downstream, not by args_has_option */

    /* a real conversion built on top, matching the intended usage pattern */
    char* argv_numeric[] = {"prog", "--port=65535"};
    u16 port = 0;
    ASSERT(args_has_option((int)ARRAY_COUNT(argv_numeric), argv_numeric, STR("--port"), STR("-p"), &val));
    ASSERT(str8_to_u16(val, &port));
    ASSERT(port == 65535);
}

static void test_validate_catches_duplicate_name(void)
{
    SECTION("args_validate: duplicate name/alias across entries is caught");

    BasicArgs args = {0};
    ArgSpec dup_name[] = {
        { STR("--name"), {0}, ArgKind_Str, &args.name, STR("V"), STR("") },
        { STR("--name"), {0}, ArgKind_Str, &args.input, STR("V"), STR("") },
    };
    Arena* arena = arena_alloc(KB(4));
    str8 err = {0};
    ASSERT(!args_validate(dup_name, ARRAY_COUNT(dup_name), arena, &err));
    ASSERT(err.size > 0);

    ArgSpec dup_alias[] = {
        { STR("--name"),   STR("-n"), ArgKind_Str, &args.name,  STR("V"), STR("") },
        { STR("--number"), STR("-n"), ArgKind_Str, &args.input, STR("V"), STR("") },
    };
    err = (str8){0};
    ASSERT(!args_validate(dup_alias, ARRAY_COUNT(dup_alias), arena, &err));
    ASSERT(err.size > 0);

    ArgSpec ok[] = {
        { STR("--name"),   STR("-n"), ArgKind_Str, &args.name,  STR("V"), STR("") },
        { STR("--number"), STR("-k"), ArgKind_Str, &args.input, STR("V"), STR("") },
    };
    ASSERT(args_validate(ok, ARRAY_COUNT(ok), arena, &err));
    ASSERT(err.size == 0);

    arena_release(arena);
}

static void test_validate_catches_null_out_and_bad_kind(void)
{
    SECTION("args_validate: a NULL out pointer or an out-of-range kind is caught");

    ArgSpec null_out[] = { { STR("--name"), {0}, ArgKind_Str, NULL, STR("V"), STR("") } };
    Arena* arena = arena_alloc(KB(4));
    str8 err = {0};
    ASSERT(!args_validate(null_out, ARRAY_COUNT(null_out), arena, &err));
    ASSERT(err.size > 0);

    BasicArgs args = {0};
    ArgSpec bad_kind[] = { { STR("--name"), {0}, (ArgKind)200, &args.name, STR("V"), STR("") } };
    err = (str8){0};
    ASSERT(!args_validate(bad_kind, ARRAY_COUNT(bad_kind), arena, &err));
    ASSERT(err.size > 0);

    arena_release(arena);
}

static void test_validate_catches_optional_before_required_positional(void)
{
    SECTION("args_validate: a required positional after an optional one is caught (ordinal matching can't tell them apart)");

    BasicArgs args = {0};
    ArgSpec bad[] = {
        { STR("FIRST"),  {0}, ArgKind_Str, &args.name,  {0}, STR(""), true, false }, /* optional */
        { STR("SECOND"), {0}, ArgKind_Str, &args.input, {0}, STR(""), true, true  }, /* required, after an optional one */
    };
    Arena* arena = arena_alloc(KB(4));
    str8 err = {0};
    ASSERT(!args_validate(bad, ARRAY_COUNT(bad), arena, &err));
    ASSERT(err.size > 0);

    ArgSpec ok[] = {
        { STR("FIRST"),  {0}, ArgKind_Str, &args.name,  {0}, STR(""), true, true  }, /* required first is fine */
        { STR("SECOND"), {0}, ArgKind_Str, &args.input, {0}, STR(""), true, false },
    };
    ASSERT(args_validate(ok, ARRAY_COUNT(ok), arena, &err));

    arena_release(arena);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"flag_str_positional",       test_flag_str_and_positional},
    {"alias_and_inline_value",    test_alias_and_inline_value},
    {"flag_rejects_inline_value", test_flag_rejects_inline_value},
    {"numeric_roundtrip",         test_numeric_kinds_roundtrip},
    {"numeric_zero_is_valid",     test_numeric_zero_is_a_valid_value},
    {"numeric_rejects_bad_value", test_numeric_kind_rejects_bad_value},
    {"unrecognized_option",       test_unrecognized_option},
    {"missing_value_for_option",  test_missing_value_for_option},
    {"empty_inline_value_falls_through", test_empty_inline_value_falls_through_to_next_token},
    {"unexpected_positional",     test_unexpected_positional},
    {"required_positional",       test_required_positional_missing},
    {"required_keyword",          test_required_keyword_missing},
    {"was_set_tracking",          test_was_set_tracking},
    {"was_set_reset_across_calls", test_was_set_reset_across_calls},
    {"err_out_cleared_on_success", test_err_out_cleared_on_success},
    {"has_flag_prescan",               test_has_flag_prescan},
    {"has_option_prescan",             test_has_option_prescan},
    {"validate_duplicate_name",        test_validate_catches_duplicate_name},
    {"validate_null_out_and_bad_kind", test_validate_catches_null_out_and_bad_kind},
    {"validate_optional_before_required_positional", test_validate_catches_optional_before_required_positional},
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
