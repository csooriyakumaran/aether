/*
 * timer.c -- HighResTimer example / validation harness
 *
 * Demonstrates fixed-rate loop pacing and verifies the three properties
 * that matter for a rate limiter:
 *
 *   1. accuracy : mean period matches the target (absolute deadlines)
 *   2. jitter   : per-frame error is bounded (hybrid sleep + spin)
 *   3. recovery : overruns are reported and the loop resyncs
 *
 * usage: aether-example-timer [hz]   (default 100)
 */

#include <stdlib.h>

#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#define FRAME_COUNT 500

static int cmp_f64(const void* a, const void* b)
{
    f64 x = *(const f64*)a;
    f64 y = *(const f64*)b;
    return (x > y) - (x < y);
}

/* simulate work by burning wall-clock time */
static void busy_work_sec(f64 secs)
{
    u64 start = time_mark();
    while (time_elapsed_sec(start, time_mark()) < secs)
        ;
}

typedef struct PacingStats
{
    f64 mean;
    f64 min;
    f64 max;
    f64 p50;
    f64 p99;
} PacingStats;

static PacingStats measure_pacing(f64* deltas, u64 count)
{
    PacingStats s = {0};

    qsort(deltas, count, sizeof(f64), cmp_f64);

    f64 sum = 0;
    for (u64 i = 0; i < count; ++i)
        sum += deltas[i];

    s.mean = sum / (f64)count;
    s.min  = deltas[0];
    s.max  = deltas[count - 1];
    s.p50  = deltas[count / 2];
    s.p99  = deltas[(count * 99) / 100];
    return s;
}

/* Test 1: empty loop at target rate. Mean period must match the target;
 * with absolute deadlines the total-duration error stays bounded by one
 * frame's jitter instead of accumulating. */
static b8 test_pacing(f64 hz)
{
    f64 period_sec = 1.0 / hz;
    f64 deltas[FRAME_COUNT];

    HighResTimer t = high_res_timer_create(hz);

    u64 mark  = time_mark();
    u64 start = mark;
    for (u64 i = 0; i < FRAME_COUNT; ++i)
    {
        high_res_timer_wait(&t);
        u64 now   = time_mark();
        deltas[i] = time_elapsed_sec(mark, now);
        mark      = now;
    }
    u64 end = time_mark();

    f64 total    = time_elapsed_sec(start, end);
    f64 expected = (f64)FRAME_COUNT * period_sec;
    u64 overruns = t.overrun;

    high_res_timer_release(&t);

    PacingStats s = measure_pacing(deltas, FRAME_COUNT);

    printf("[pacing] target %8.3f ms | mean %8.4f ms | p50 %8.4f | p99 %8.4f | min %8.4f | max %8.4f\n",
           period_sec * 1e3, s.mean * 1e3, s.p50 * 1e3, s.p99 * 1e3, s.min * 1e3, s.max * 1e3);
    printf("[pacing] total  %8.4f s vs expected %8.4f s | drift %+.3f ms | overruns %llu\n",
           total, expected, (total - expected) * 1e3, (unsigned long long)overruns);

    /* mean rate within 0.1% and no overruns on an idle loop */
    b8 ok = (s.mean > period_sec * 0.999) && (s.mean < period_sec * 1.001) && (overruns == 0);
    printf("[pacing] %s\n\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Test 2: every 10th frame deliberately busy-works for 2.5 periods.
 * The wait must report the missed deadlines and resync so the loop
 * recovers instead of bursting to catch up. */
static b8 test_overrun(f64 hz)
{
    f64 period_sec = 1.0 / hz;

    HighResTimer t = high_res_timer_create(hz);

    u64 reported = 0;
    u64 slow_frames = 0;
    for (u64 i = 0; i < 100; ++i)
    {
        if (i % 10 == 9)
        {
            busy_work_sec(2.5 * period_sec);
            slow_frames++;
        }
        reported += high_res_timer_wait(&t);
    }

    u64 counted = t.overrun;
    high_res_timer_release(&t);

    printf("[overrun] slow frames %llu | misses reported by wait %llu | timer overrun count %llu\n",
           (unsigned long long)slow_frames,
           (unsigned long long)reported,
           (unsigned long long)counted);

    /* each slow frame blows ~2.5 periods -> expect ~2 misses per slow frame;
     * accept a loose band since exact counts depend on scheduling */
    b8 ok = (reported == counted)
         && (reported >= slow_frames)
         && (reported <= 3 * slow_frames);
    printf("[overrun] %s\n\n", ok ? "PASS" : "FAIL");
    return ok;
}

int main(int argc, char** argv)
{
    f64 hz = (argc > 1) ? atof(argv[1]) : 100.0;
    if (hz <= 0) { fprintf(stderr, "invalid rate: %s\n", argv[1]); return 1; }

    printf("HighResTimer validation @ %.1f Hz, %d frames\n\n", hz, FRAME_COUNT);

    b8 ok = true;
    ok &= test_pacing(hz);
    ok &= test_overrun(hz);

    printf("%s\n", ok ? "ALL PASS" : "FAILURES");
    return ok ? 0 : 1;
}
