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
    ASSERT(t.armed == 0);

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
    ASSERT(t.armed == 0);

    u64 misses = high_res_timer_wait(&t);
    ASSERT(misses == 0);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"alloc_basic",                    test_alloc_basic},
    {"wait_without_arm_is_safe",       test_wait_without_arm_is_safe},
    {"arm_sets_state",                 test_arm_sets_state},
    {"create_convenience",             test_create_convenience},
    {"wait_paces_interval",            test_wait_paces_interval},
    {"wait_reports_overrun",           test_wait_reports_overrun},
    {"rearm_resets_overrun_and_phase", test_rearm_resets_overrun_and_phase},
    {"set_rate_changes_period_only",   test_set_rate_changes_period_only},
    {"release_clears_state",           test_release_clears_state},
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
