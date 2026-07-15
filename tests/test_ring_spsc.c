#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* SPSC ring stress driven through the aether thread API.

   test_ring.c's spsc_stress runs the same producer/consumer protocol on raw
   _beginthreadex/pthread shims because it predates the thread API; this file
   is the same idea expressed in the public API -- thread_create, thread_join
   (exit codes), thread_set_priority, thread_yield -- so it doubles as the
   sufficiency test for that API. Two priority variants invert which side
   waits: a hot consumer forces the producer through the full-ring backoff,
   a hot producer forces the consumer through the empty-ring backoff. A test
   that only ever waits on one side has only tested half the protocol.

   The harness ASSERT is not thread-safe (g_checks++ is unsynchronized), so
   workers record into their SpscCtx and return an exit code; only main()
   asserts, after joining. thread_join is the synchronization point that
   makes the ctx results safely readable.

   Caveat: a pass on x64 is necessary, not sufficient -- the hardware can't
   produce most acquire/release violations. Once the POSIX port lands, this
   file under `clang -fsanitize=thread` is the real referee. */

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

/* --- thread API contract --------------------------------------------------*/

static int return_code_fn(void* user) { return *(int*)user; }

static void test_thread_api(void)
{
    SECTION("threads: create/join contract -- exit codes, null-handle error paths");

    /* NULL fn refuses to create; the resulting {0} thread hits every error path */
    Thread t0 = thread_create(NULL, NULL);
    ASSERT(t0.handle == NULL);

    int code = 1234;
    ASSERT(!thread_join(&t0, &code));
    ASSERT(code == 1234);                          /* out_code untouched on failure */
    ASSERT(!thread_join(NULL, &code));
    ASSERT(!thread_set_priority(&t0, ThreadPriority_High));

    /* exit code round-trips through join, including negative values */
    int want = 42;
    Thread t1 = thread_create(return_code_fn, &want);
    ASSERT(t1.handle != NULL);
    ASSERT(thread_join(&t1, &code));
    ASSERT(code == 42);
    ASSERT(t1.handle == NULL);                     /* join cleared the handle */
    ASSERT(!thread_join(&t1, &code));              /* double join is an error, not a hang */

    want = -7;
    Thread t2 = thread_create(return_code_fn, &want);
    ASSERT(thread_join(&t2, &code));
    ASSERT(code == -7);                            /* sign survives the DWORD round-trip */

    /* NULL out_code means "join, discard the code" */
    want = 1;
    Thread t3 = thread_create(return_code_fn, &want);
    ASSERT(thread_join(&t3, NULL));
}

static void test_thread_sleep(void)
{
    SECTION("threads: sleep_ms sleeps at least ms (no upper-bound assert; CI varies)");

    u64 start = time_mark();
    thread_sleep_ms(20);
    f64 elapsed = time_elapsed_sec(start, time_mark());
    ASSERT(elapsed >= 0.015);   /* generous: catches an instant return, not jitter */
}

/* --- SPSC stress ----------------------------------------------------------*/

static u64 xorshift64(u64* s)
{
    u64 x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

#define SPSC_DATA_SEED  0x9E3779B97F4A7C15ull  /* shared: producer emits, consumer expects */
#define SPSC_MAX_CHUNK  4096

typedef struct SpscCtx
{
    RingBuffer* rb;
    u64 total;      /* bytes to move */
    u64 chunk_seed; /* per-thread: chunk-size sequence */
    /* results, written by the worker, read by main after join */
    u64 moved;
    u64 fail_pos;
    u8  fail_got;
    u8  fail_want;
} SpscCtx;

static int spsc_producer(void* user)
{
    SpscCtx* ctx = (SpscCtx*)user;
    u64 data      = SPSC_DATA_SEED;
    u64 chunk_rng = ctx->chunk_seed;
    u8  buf[SPSC_MAX_CHUNK];

    while (ctx->moved < ctx->total)
    {
        u64 chunk = 1 + (xorshift64(&chunk_rng) % SPSC_MAX_CHUNK);
        if (ctx->moved + chunk > ctx->total) chunk = ctx->total - ctx->moved;

        /* generate BEFORE the write attempt: the stream position must advance
           exactly once per byte moved, even across retries */
        for (u64 i = 0; i < chunk; ++i) buf[i] = (u8)xorshift64(&data);

        /* full ring: yield until the consumer frees space (SwitchToThread
           lets lower-priority threads run, so a hot producer can't starve
           a normal consumer here) */
        while (!ring_buffer_write(ctx->rb, buf, chunk))
            thread_yield();

        ctx->moved += chunk;
    }
    return 0;
}

static int spsc_consumer(void* user)
{
    SpscCtx* ctx = (SpscCtx*)user;
    u64 expect    = SPSC_DATA_SEED;
    u64 chunk_rng = ctx->chunk_seed;
    u8  buf[SPSC_MAX_CHUNK];
    int mismatched = 0;

    while (ctx->moved < ctx->total)
    {
        u64 want = 1 + (xorshift64(&chunk_rng) % SPSC_MAX_CHUNK);
        if (ctx->moved + want > ctx->total) want = ctx->total - ctx->moved;

        /* alternate the copying and zero-copy consume paths */
        const u8* got  = NULL;
        u64       held = 0;   /* nonzero: bytes held via peek, advance after verify */

        if (xorshift64(&chunk_rng) & 1)
        {
            bytes_view v = ring_buffer_peek(ctx->rb, want);
            if (!v.size) { thread_yield(); continue; }
            got  = v.data;
            held = v.size;
        }
        else
        {
            if (!ring_buffer_read(ctx->rb, buf, want)) { thread_yield(); continue; }
            got = buf;
        }

        for (u64 i = 0; i < want; ++i)
        {
            u8 w = (u8)xorshift64(&expect);
            if (got[i] != w && !mismatched)        /* record only the first mismatch */
            {
                mismatched     = 1;
                ctx->fail_pos  = ctx->moved + i;
                ctx->fail_got  = got[i];
                ctx->fail_want = w;
            }
        }

        /* verify BEFORE advancing: the view dies at advance_read */
        if (held) ring_buffer_advance_read(ctx->rb, held);

        ctx->moved += want;
    }
    return mismatched;   /* exit code carries pass/fail through thread_join */
}

static void run_spsc(ThreadPriority prod_p, ThreadPriority cons_p)
{
    RingBuffer rb = ring_buffer_alloc(KB(64));

    SpscCtx p = {0};
    SpscCtx c = {0};
    p.rb = &rb;  p.total = rb.size * 256 + 123;  p.chunk_seed = 0xBADC0FFEE0DDF00Dull;
    c.rb = &rb;  c.total = p.total;              c.chunk_seed = 0x0123456789ABCDEFull;

    Thread tc = thread_create(spsc_consumer, &c);
    Thread tp = thread_create(spsc_producer, &p);
    ASSERT(tc.handle != NULL);
    ASSERT(tp.handle != NULL);

    /* set after create: priority shapes the contention pattern, it is not a
       correctness requirement, so a brief run at Normal first is fine */
    ASSERT(thread_set_priority(&tp, prod_p));
    ASSERT(thread_set_priority(&tc, cons_p));

    int p_code = -1, c_code = -1;
    ASSERT(thread_join(&tp, &p_code));
    ASSERT(thread_join(&tc, &c_code));

    ASSERT(p_code == 0);
    ASSERT(c_code == 0);
    ASSERT(p.moved == p.total);
    ASSERT(c.moved == c.total);
    if (c_code != 0)
        fprintf(stderr, "  first mismatch at byte %llu: got 0x%02X want 0x%02X\n",
                (unsigned long long)c.fail_pos, c.fail_got, c.fail_want);

    ring_buffer_release(&rb);
}

static void test_spsc_consumer_hot(void)
{
    SECTION("ring+threads: SPSC, hot consumer -- producer exercises full-ring backoff");
    run_spsc(ThreadPriority_Normal, ThreadPriority_High);
}

static void test_spsc_producer_hot(void)
{
    SECTION("ring+threads: SPSC, hot producer -- consumer exercises empty-ring backoff");
    run_spsc(ThreadPriority_High, ThreadPriority_Normal);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"thread_api",   test_thread_api},
    {"thread_sleep", test_thread_sleep},
    {"consumer_hot", test_spsc_consumer_hot},
    {"producer_hot", test_spsc_producer_hot},
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
