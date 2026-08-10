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

static void test_alloc_basic(void)
{
    SECTION("high_res_timer_alloc: resource ready, clock inert");

    HighResTimer t = high_res_timer_alloc(100.0);
    ASSERT(t.os_timer != NULL);
    ASSERT(t.period_ticks > 0);
    ASSERT(t.spin_margin > 0);
    ASSERT(t.next_deadline == 0);
    ASSERT(t.overrun == 0);
    ASSERT(t.wake_time == 0);
    ASSERT(t.armed == 0);

    high_res_timer_release(&t);
}

static void test_alloc_clamps_spin_margin_at_high_rate(void)
{
    SECTION("high_res_timer_alloc: spin_margin is clamped to half the period at high rates");

    /* 5kHz -> period ~= 200us, well under the fixed 1ms margin used at low rates */
    HighResTimer t = high_res_timer_alloc(5000.0);
    ASSERT(t.spin_margin > 0);
    ASSERT(t.spin_margin <= t.period_ticks / 2);

    high_res_timer_release(&t);
}

static void test_alloc_uses_fixed_margin_at_low_rate(void)
{
    SECTION("high_res_timer_alloc: spin_margin stays at 1ms once the period is large enough");

    HighResTimer t = high_res_timer_alloc(100.0);  /* period = 10ms, well above 2ms */
    u64 expected_margin = os_time_frequency() / 1000;
    ASSERT(t.spin_margin == expected_margin);

    high_res_timer_release(&t);
}

static void test_wait_without_arm_is_safe(void)
{
    SECTION("high_res_timer_wait: unarmed timer is a harmless no-op");

    HighResTimer t = high_res_timer_alloc(100.0);
    u64 misses = high_res_timer_wait(&t);
    ASSERT(misses == 0);
    ASSERT(t.overrun == 0);
    ASSERT(t.next_deadline == 0);   /* wait() must not touch it while unarmed */
    ASSERT(t.wake_time == 0);

    high_res_timer_release(&t);
}

static void test_arm_sets_state(void)
{
    SECTION("high_res_timer_arm: starts the clock and hands back the start mark");

    HighResTimer t = high_res_timer_alloc(100.0);
    u64 before = time_mark();
    u64 mark   = high_res_timer_arm(&t);
    u64 after  = time_mark();

    ASSERT(t.armed == 1);
    ASSERT(t.overrun == 0);
    ASSERT(t.next_deadline == mark);
    ASSERT(mark >= before && mark <= after);

    high_res_timer_release(&t);
}

static void test_create_convenience(void)
{
    SECTION("high_res_timer_create: alloc + arm in one call");

    HighResTimer t = high_res_timer_create(100.0);
    ASSERT(t.os_timer != NULL);
    ASSERT(t.period_ticks > 0);
    ASSERT(t.armed == 1);
    ASSERT(t.next_deadline != 0);

    high_res_timer_release(&t);
}

static void test_wait_paces_interval(void)
{
    SECTION("high_res_timer_wait: N waits at 100Hz take roughly N periods");

    const int frames = 20;
    HighResTimer t = high_res_timer_create(100.0);

    u64 start = time_mark();
    for (int i = 0; i < frames; ++i) high_res_timer_wait(&t);
    f64 elapsed = time_elapsed_sec(start, time_mark());

    f64 expected = frames * 0.010;
    ASSERT(elapsed > expected * 0.5 && elapsed < expected * 2.0);
    ASSERT(t.overrun == 0);

    high_res_timer_release(&t);
}

static void test_wait_reports_overrun(void)
{
    SECTION("high_res_timer_wait: a late call reports and accumulates misses");

    HighResTimer t = high_res_timer_create(1000.0);  /* 1ms period, easy to overshoot */
    thread_sleep_ms(30);                              /* blow past several periods */

    u64 misses = high_res_timer_wait(&t);
    ASSERT(misses > 0);
    ASSERT(t.overrun == misses);

    high_res_timer_release(&t);
}

static void test_wait_sets_wake_time_on_success(void)
{
    SECTION("high_res_timer_wait: wake_time samples the actual spin-exit instant, at or after the deadline");

    HighResTimer t = high_res_timer_create(100.0);

    u64 before   = time_mark();
    u64 deadline = t.next_deadline + t.period_ticks;
    high_res_timer_wait(&t);
    u64 after    = time_mark();

    ASSERT(t.wake_time >= deadline);   /* spin loop never exits before the deadline */
    ASSERT(t.wake_time >= before && t.wake_time <= after);

    high_res_timer_release(&t);
}

static void test_wait_sets_wake_time_on_overrun(void)
{
    SECTION("high_res_timer_wait: a missed deadline sets wake_time to the exact call-time sample");

    HighResTimer t = high_res_timer_create(1000.0);  /* 1ms period, easy to overshoot */
    thread_sleep_ms(30);

    u64 before = time_mark();
    u64 misses = high_res_timer_wait(&t);
    u64 after  = time_mark();

    ASSERT(misses > 0);
    ASSERT(t.wake_time == t.next_deadline);  /* miss path sets both to the same sampled "now" */
    ASSERT(t.wake_time >= before && t.wake_time <= after);

    high_res_timer_release(&t);
}

static void test_rearm_resets_overrun_and_phase(void)
{
    SECTION("high_res_timer_arm: re-arming clears accumulated overrun and resyncs phase");

    HighResTimer t = high_res_timer_create(1000.0);
    thread_sleep_ms(30);
    high_res_timer_wait(&t);
    ASSERT(t.overrun > 0);

    u64 before = time_mark();
    u64 mark   = high_res_timer_arm(&t);
    ASSERT(t.overrun == 0);
    ASSERT(t.armed == 1);
    ASSERT(mark >= before);

    high_res_timer_release(&t);
}

static void test_set_rate_changes_period_only(void)
{
    SECTION("high_res_timer_set_rate: changes pacing without touching phase or armed state");

    HighResTimer t = high_res_timer_create(100.0);
    u64 deadline_before = t.next_deadline;
    b32 armed_before    = t.armed;
    u64 period_before   = t.period_ticks;

    high_res_timer_set_rate(&t, 50.0);   /* slower: fewer ticks/sec -> bigger period */
    ASSERT(t.period_ticks > period_before);
    ASSERT(t.next_deadline == deadline_before);
    ASSERT(t.armed == armed_before);

    high_res_timer_release(&t);
}

static void test_release_clears_state(void)
{
    SECTION("high_res_timer_release: zeroes state and wait() becomes a safe no-op");

    HighResTimer t = high_res_timer_create(100.0);
    high_res_timer_release(&t);

    ASSERT(t.os_timer == NULL);
    ASSERT(t.period_ticks == 0);
    ASSERT(t.next_deadline == 0);
    ASSERT(t.overrun == 0);
    ASSERT(t.wake_time == 0);
    ASSERT(t.armed == 0);

    u64 misses = high_res_timer_wait(&t);
    ASSERT(misses == 0);
}

static void test_datetime_epoch_roundtrip(void)
{
    SECTION("datetime_to_ns_since_epoch / datetime_from_ns_since_epoch: known reference points");

    datetime epoch = {0}; epoch.year = 1970; epoch.month = 1; epoch.day = 1;
    u64 ns = 0xDEADBEEF;
    ASSERT(datetime_to_ns_since_epoch(epoch, &ns) && ns == 0);

    datetime back = datetime_from_ns_since_epoch(0);
    ASSERT(back.year == 1970 && back.month == 1 && back.day == 1);
    ASSERT(back.hour == 0 && back.minute == 0 && back.second == 0 && back.ns == 0);

    /* 2000-01-01T00:00:00Z is a well-known reference: 946684800 sec since epoch */
    datetime y2k = {0}; y2k.year = 2000; y2k.month = 1; y2k.day = 1;
    ASSERT(datetime_to_ns_since_epoch(y2k, &ns) && ns == 946684800ull * 1000000000ull);

    datetime y2k_back = datetime_from_ns_since_epoch(946684800ull * 1000000000ull);
    ASSERT(y2k_back.year == 2000 && y2k_back.month == 1 && y2k_back.day == 1);

    /* arbitrary date with sub-second precision, round-tripped both directions */
    datetime dt; dt.year = 2026; dt.month = 8; dt.day = 10;
    dt.hour = 13; dt.minute = 45; dt.second = 30; dt.ns = 123456789ull;
    ASSERT(datetime_to_ns_since_epoch(dt, &ns));
    datetime rt = datetime_from_ns_since_epoch(ns);
    ASSERT(rt.year == dt.year && rt.month == dt.month && rt.day == dt.day);
    ASSERT(rt.hour == dt.hour && rt.minute == dt.minute && rt.second == dt.second);
    ASSERT(rt.ns == dt.ns);
}

static void test_datetime_leap_years(void)
{
    SECTION("datetime_to_ns_since_epoch: Gregorian leap year rule (div 4, not 100, except div 400)");

    datetime dt = {0}; dt.month = 2; dt.day = 29;
    u64 ns;

    dt.year = 2000; ASSERT(datetime_to_ns_since_epoch(dt, &ns));   /* div 400 -> leap */
    dt.year = 2024; ASSERT(datetime_to_ns_since_epoch(dt, &ns));   /* div 4, not 100 -> leap */
    dt.year = 2100; ASSERT(!datetime_to_ns_since_epoch(dt, &ns));  /* div 100, not 400 -> not leap */
    dt.year = 2026; ASSERT(!datetime_to_ns_since_epoch(dt, &ns));  /* not div 4 at all */

    /* the same boundary shows up correctly on decode: day after Feb 28 in a
     * non-leap century year is Mar 1, not Feb 29 (2100 chosen over 1900 so the
     * date is still post-epoch -- 1900 would be rejected for that reason alone) */
    datetime nonleap_mar1 = {0}; nonleap_mar1.year = 2100; nonleap_mar1.month = 3; nonleap_mar1.day = 1;
    ASSERT(datetime_to_ns_since_epoch(nonleap_mar1, &ns));
    datetime back = datetime_from_ns_since_epoch(ns - 1000000000ull); /* one second earlier */
    ASSERT(back.year == 2100 && back.month == 2 && back.day == 28 && back.second == 59);
}

static void test_datetime_to_ns_since_epoch_rejects_invalid(void)
{
    SECTION("datetime_to_ns_since_epoch: rejects out-of-range calendar fields");

    u64 ns;
    datetime base = {0}; base.year = 2026; base.month = 1; base.day = 1;

    datetime bad_month = base; bad_month.month = 13;
    ASSERT(!datetime_to_ns_since_epoch(bad_month, &ns));
    datetime zero_month = base; zero_month.month = 0;
    ASSERT(!datetime_to_ns_since_epoch(zero_month, &ns));

    datetime bad_day = base; bad_day.month = 4; bad_day.day = 31;  /* April has 30 days */
    ASSERT(!datetime_to_ns_since_epoch(bad_day, &ns));
    datetime zero_day = base; zero_day.day = 0;
    ASSERT(!datetime_to_ns_since_epoch(zero_day, &ns));

    datetime bad_hour = base; bad_hour.hour = 24;
    ASSERT(!datetime_to_ns_since_epoch(bad_hour, &ns));
    datetime bad_minute = base; bad_minute.minute = 60;
    ASSERT(!datetime_to_ns_since_epoch(bad_minute, &ns));
    datetime bad_second = base; bad_second.second = 60;
    ASSERT(!datetime_to_ns_since_epoch(bad_second, &ns));
    datetime bad_ns = base; bad_ns.ns = 1000000000ull;  /* one past 999,999,999 */
    ASSERT(!datetime_to_ns_since_epoch(bad_ns, &ns));

    datetime pre_epoch = {0}; pre_epoch.year = 1969; pre_epoch.month = 12; pre_epoch.day = 31;
    pre_epoch.hour = 23; pre_epoch.minute = 59; pre_epoch.second = 59;
    ASSERT(!datetime_to_ns_since_epoch(pre_epoch, &ns));  /* pre-1970: not representable as unsigned */

    datetime far_future = {0}; far_future.year = 9999; far_future.month = 1; far_future.day = 1;
    ASSERT(!datetime_to_ns_since_epoch(far_future, &ns)); /* past the u64-ns representable range */

    ASSERT(!datetime_to_ns_since_epoch(base, NULL));  /* NULL out-param */
}

static void test_datetime_now_is_plausible(void)
{
    SECTION("datetime_now: returns a well-formed, internally-consistent value");

    datetime now = datetime_now();
    ASSERT(now.year >= 2020 && now.year <= 2100);  /* sanity band, not a precise check */
    ASSERT(now.month >= 1 && now.month <= 12);
    ASSERT(now.day >= 1 && now.day <= days_in_month_(now.year, now.month));
    ASSERT(now.hour < 24 && now.minute < 60 && now.second < 60);
    ASSERT(now.ns < 1000000000ull);

    /* whatever the OS clock reported must itself pass the same validation
     * datetime_to_ns_since_epoch applies to hand-built values */
    u64 ns;
    ASSERT(datetime_to_ns_since_epoch(now, &ns));
}

static void test_wall_clock_ns_is_consistent_with_datetime_now(void)
{
    SECTION("wall_clock_ns: raw epoch-ns agrees with datetime_now()'s decomposition");

    u64 before = wall_clock_ns();
    datetime now = datetime_now();
    u64 after = wall_clock_ns();

    u64 now_ns;
    ASSERT(datetime_to_ns_since_epoch(now, &now_ns));

    /* three back-to-back wall-clock reads, microseconds apart -- must be
     * non-decreasing and land in the same narrow window (generous 1s band to
     * stay clear of scheduler jitter, not because that much drift is expected) */
    ASSERT(before <= now_ns);
    ASSERT(now_ns <= after);
    ASSERT(after - before < 1000000000ull);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"alloc_basic",                        test_alloc_basic},
    {"alloc_clamps_spin_margin_high_rate", test_alloc_clamps_spin_margin_at_high_rate},
    {"alloc_uses_fixed_margin_low_rate",   test_alloc_uses_fixed_margin_at_low_rate},
    {"wait_without_arm_is_safe",           test_wait_without_arm_is_safe},
    {"arm_sets_state",                     test_arm_sets_state},
    {"create_convenience",                 test_create_convenience},
    {"wait_paces_interval",                test_wait_paces_interval},
    {"wait_reports_overrun",               test_wait_reports_overrun},
    {"wait_sets_wake_time_on_success",     test_wait_sets_wake_time_on_success},
    {"wait_sets_wake_time_on_overrun",     test_wait_sets_wake_time_on_overrun},
    {"rearm_resets_overrun_and_phase",     test_rearm_resets_overrun_and_phase},
    {"set_rate_changes_period_only",       test_set_rate_changes_period_only},
    {"release_clears_state",               test_release_clears_state},
    {"datetime_epoch_roundtrip",           test_datetime_epoch_roundtrip},
    {"datetime_leap_years",                test_datetime_leap_years},
    {"datetime_to_ns_rejects_invalid",     test_datetime_to_ns_since_epoch_rejects_invalid},
    {"datetime_now_is_plausible",          test_datetime_now_is_plausible},
    {"wall_clock_ns_consistent",           test_wall_clock_ns_is_consistent_with_datetime_now},
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
