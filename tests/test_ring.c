#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

/* Same harness contract as test_arenas.c: AETHER_NO_ASSERT keeps the library
   from claiming the ASSERT name so our ASSERT below records a failure and keeps
   going instead of trapping on the first one. */

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

static b8 is_power_of_two(u64 v) { return v != 0 && (v & (v - 1)) == 0; }

/* The structural test: the two mapped halves must alias the SAME physical
   pages. Everything else can pass even if the mirror is broken (a plain
   modulo buffer would satisfy a naive round-trip), so this runs first. */
static void test_mirror(void)
{
    SECTION("ring: upper half mirrors lower half (shared physical pages)");

    RingBuffer rb = ring_buffer_alloc(KB(4));
    ASSERT(rb.base != NULL);

    rb.base[0] = 0xAB;
    ASSERT(rb.base[rb.size] == 0xAB);          /* write low, observe high */

    rb.base[rb.size + 5] = 0xCD;
    ASSERT(rb.base[5] == 0xCD);                /* write high, observe low */

    ring_buffer_release(&rb);
}

static void test_alloc(void)
{
    SECTION("ring: alloc rounds size to a power-of-two >= granularity; release zeroes");

    u64 gran = os_mem_pagegranularity();

    /* a tiny request rounds up to the allocation granularity (64 KiB) */
    RingBuffer small = ring_buffer_alloc(100);
    ASSERT(small.size == gran);
    ASSERT(is_power_of_two(small.size));
    ASSERT(small.size >= gran);
    ASSERT(small.read == 0 && small.write == 0);

    ring_buffer_release(&small);
    ASSERT(small.base == NULL);
    ASSERT(small.size == 0 && small.read == 0 && small.write == 0);

    /* a request above granularity rounds to the next power of two */
    RingBuffer big = ring_buffer_alloc(MB(1));
    ASSERT(big.size == MB(1));                 /* MB(1) is already a power of two */
    ASSERT(is_power_of_two(big.size));
    ring_buffer_release(&big);
}

static void test_roundtrip(void)
{
    SECTION("ring: basic write/read round-trip preserves bytes and advances counters");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    u8 src[256], dst[256];
    for (u32 i = 0; i < 256; ++i) src[i] = (u8)i;

    ASSERT(ring_buffer_write(&rb, src, 256));
    ASSERT(rb.write == 256 && rb.read == 0);

    ASSERT(ring_buffer_read(&rb, dst, 256));
    ASSERT(rb.read == 256);
    ASSERT(memcmp(src, dst, 256) == 0);

    ring_buffer_release(&rb);
}

/* Prove a single-memcpy write and a contiguous peek work ACROSS the wrap.
   Park the (empty) buffer's indices near the top by setting them directly --
   the public monotonic counters make this clean and size-independent. */
static void test_seam(void)
{
    SECTION("ring: write/peek/read straddling the wrap stay contiguous");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    u64 k = 8;
    rb.read = rb.write = rb.size - k;          /* empty; write_idx = size - k */

    u8 payload[32], out[32];
    for (u32 i = 0; i < 32; ++i) payload[i] = (u8)(0x40 + i);

    ASSERT(ring_buffer_write(&rb, payload, 32)); /* spans [size-8 .. size+24) */
    ASSERT(rb.base[0] == payload[k]);            /* tail visible at base[0] via mirror */

    bytes_view v = ring_buffer_peek(&rb, 32);
    ASSERT(v.size == 32);
    ASSERT(memcmp(v.data, payload, 32) == 0);    /* one contiguous span across the seam */

    ASSERT(ring_buffer_read(&rb, out, 32));
    ASSERT(memcmp(out, payload, 32) == 0);

    ring_buffer_release(&rb);
}

static void test_peek_no_consume(void)
{
    SECTION("ring: peek is idempotent and does not advance read");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    u8 src[64];
    for (u32 i = 0; i < 64; ++i) src[i] = (u8)(i * 3);
    ASSERT(ring_buffer_write(&rb, src, 64));

    u64 read_before = rb.read;
    bytes_view a = ring_buffer_peek(&rb, 64);
    bytes_view b = ring_buffer_peek(&rb, 64);
    ASSERT(a.data == b.data && a.size == b.size); /* idempotent */
    ASSERT(rb.read == read_before);               /* did not consume */
    ASSERT(memcmp(a.data, src, 64) == 0);

    u8 out[32];
    ASSERT(ring_buffer_read(&rb, out, 32));        /* now consume half */
    bytes_view c = ring_buffer_peek(&rb, 32);
    ASSERT(c.data != a.data);                      /* peek window moved forward */
    ASSERT(memcmp(c.data, src + 32, 32) == 0);

    ring_buffer_release(&rb);
}

static void test_guards(void)
{
    SECTION("ring: read/write/peek reject out-of-bounds requests without mutating state");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    u8 buf[64] = {0};

    /* read from an empty buffer fails and leaves read untouched */
    ASSERT(!ring_buffer_read(&rb, buf, 1));
    ASSERT(rb.read == 0);

    /* peek beyond what is available returns a zeroed view */
    bytes_view e = ring_buffer_peek(&rb, 1);
    ASSERT(e.data == NULL && e.size == 0);

    /* a request larger than the whole capacity is rejected up front (the size
       guard runs before the memcpy, so passing a small buf is safe here) */
    ASSERT(!ring_buffer_write(&rb, buf, rb.size + 1));
    ASSERT(rb.write == 0);

    /* free-space boundary: pretend the buffer is all but 10 bytes full */
    rb.read  = 0;
    rb.write = rb.size - 10;
    ASSERT(!ring_buffer_write(&rb, buf, 11));      /* 11 > 10 free -> reject */
    ASSERT(rb.write == rb.size - 10);              /* unchanged */
    ASSERT(ring_buffer_write(&rb, buf, 10));       /* exactly fills */
    ASSERT(rb.write == rb.size);                   /* now full: write - read == size */
    ASSERT(!ring_buffer_write(&rb, buf, 1));       /* full -> reject */

    ring_buffer_release(&rb);
}

static void test_available_advance(void)
{
    SECTION("ring: available tracks write-read; advance_read consumes without copying");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    ASSERT(ring_buffer_available(&rb) == 0);         /* empty */

    u8 src[128];
    for (u32 i = 0; i < 128; ++i) src[i] = (u8)(i + 1);
    ASSERT(ring_buffer_write(&rb, src, 128));
    ASSERT(ring_buffer_available(&rb) == 128);       /* tracks bytes written */

    /* zero-copy consume: peek a contiguous view, use it in place, then advance */
    bytes_view v = ring_buffer_peek(&rb, 50);
    ASSERT(v.size == 50);
    ASSERT(memcmp(v.data, src, 50) == 0);
    ASSERT(ring_buffer_advance_read(&rb, 50));        /* release exactly what was used */
    ASSERT(rb.read == 50);
    ASSERT(ring_buffer_available(&rb) == 78);         /* 128 - 50 */

    bytes_view w = ring_buffer_peek(&rb, 78);
    ASSERT(w.size == 78);
    ASSERT(memcmp(w.data, src + 50, 78) == 0);        /* window moved past the advance */

    /* advance past what is available is rejected and leaves read untouched */
    ASSERT(!ring_buffer_advance_read(&rb, 79));       /* 79 > 78 available */
    ASSERT(rb.read == 50);
    ASSERT(ring_buffer_available(&rb) == 78);

    ASSERT(ring_buffer_advance_read(&rb, 78));         /* advancing exactly to empty is allowed */
    ASSERT(ring_buffer_available(&rb) == 0);
    ASSERT(rb.read == rb.write);

    ring_buffer_release(&rb);

    /* guards: zero/false on a released or NULL buffer, never a deref */
    ASSERT(ring_buffer_available(&rb) == 0);
    ASSERT(ring_buffer_available(NULL) == 0);
    ASSERT(!ring_buffer_advance_read(NULL, 1));
}

/* Pump far more than `size` bytes through with chunk sizes that do not divide
   `size`, comparing a deterministic producer stream against the consumer.
   This exercises index masking and counter advance over many wraps. */
static void test_stream(void)
{
    SECTION("ring: streaming many wraps with non-dividing chunks preserves byte order");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    u64 total    = (u64)rb.size * 10 + 123;    /* many wraps, deliberately unaligned */
    u64 produced = 0, consumed = 0;
    u8  next_w = 0, next_r = 0;                 /* expected stream is 0,1,2,... (mod 256) */
    u8  buf[1000];
    b8  order_ok = true;

    while (consumed < total)
    {
        if (produced < total)
        {
            u64 chunk = 37;
            if (produced + chunk > total) chunk = total - produced;
            for (u64 i = 0; i < chunk; ++i) buf[i] = next_w++;
            if (ring_buffer_write(&rb, buf, chunk)) produced += chunk;
        }

        u64 want = 53;
        if (consumed + want > produced) want = produced - consumed;
        if (want && ring_buffer_read(&rb, buf, want))
        {
            for (u64 i = 0; i < want; ++i)
                if (buf[i] != next_r++) { order_ok = false; break; }
            consumed += want;
        }
    }

    ASSERT(order_ok);
    ASSERT(consumed == total);

    ring_buffer_release(&rb);
}

/* The one property you can't observe on a normal run: the monotonic u64
   counters must keep masking correctly when they roll over 2^64. Park them
   just below the max and push past zero. (This also straddles the seam.) */
static void test_counter_wrap(void)
{
    SECTION("ring: read/write counters survive u64 overflow");

    RingBuffer rb = ring_buffer_alloc(KB(4));

    rb.read = rb.write = (u64)-20;             /* 20 short of 2^64 */

    u8 src[64], dst[64];
    for (u32 i = 0; i < 40; ++i) src[i] = (u8)(0x80 + i);

    ASSERT(ring_buffer_write(&rb, src, 40));
    ASSERT(rb.write == (u64)20);               /* -20 + 40 wraps to 20 */

    ASSERT(ring_buffer_read(&rb, dst, 40));
    ASSERT(rb.read == (u64)20);
    ASSERT(memcmp(src, dst, 40) == 0);

    ring_buffer_release(&rb);
}

/* --- SPSC stress ----------------------------------------------------------
   Producer and consumer on real OS threads, moving ~32 MB through a 64 KiB
   ring in randomly sized chunks. Both threads generate the same PRNG byte
   stream from a shared seed; the consumer verifies every byte. A stale read
   (old lap data) or torn publish shows up as a mismatch because the stream's
   period does not divide the ring size -- unlike the 0,1,2.. pattern in
   test_stream, which would alias across laps.

   The harness ASSERT is not thread-safe (g_checks++ is unsynchronized), so
   workers record into SpscCtx and only main() asserts, after joining.

   Caveat: a pass on x64 is necessary, not sufficient -- the hardware can't
   produce most acquire/release violations. The pthread branch below exists
   so this same file runs under `clang -fsanitize=thread` on Linux/WSL,
   which checks the reasoning, not just the outcome. */

#ifdef _WIN32
    #include <process.h>   /* _beginthreadex; windows.h already in via impl */
    typedef HANDLE test_thread;
    #define THREAD_FN(name) static unsigned __stdcall name(void* arg)
    #define THREAD_RETURN() return 0
    static test_thread test_thread_start(unsigned (__stdcall *fn)(void*), void* arg)
    {
        return (test_thread)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
    }
    static void test_thread_join(test_thread t)
    {
        WaitForSingleObject(t, INFINITE);
        CloseHandle(t);
    }
    static void test_thread_yield(void) { SwitchToThread(); }
#else
    #include <pthread.h>
    #include <sched.h>
    typedef pthread_t test_thread;
    #define THREAD_FN(name) static void* name(void* arg)
    #define THREAD_RETURN() return NULL
    typedef void* (*test_thread_entry)(void*);
    static test_thread test_thread_start(test_thread_entry fn, void* arg)
    {
        pthread_t t;
        pthread_create(&t, NULL, fn, arg);
        return t;
    }
    static void test_thread_join(test_thread t) { pthread_join(t, NULL); }
    static void test_thread_yield(void) { sched_yield(); }
#endif

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
    b8  ok;
    u64 fail_pos;
    u8  fail_got;
    u8  fail_want;
} SpscCtx;

THREAD_FN(spsc_producer)
{
    SpscCtx* ctx = (SpscCtx*)arg;
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

        /* full ring: spin until the consumer frees space */
        while (!ring_buffer_write(ctx->rb, buf, chunk))
            test_thread_yield();

        ctx->moved += chunk;
    }
    THREAD_RETURN();
}

THREAD_FN(spsc_consumer)
{
    SpscCtx* ctx = (SpscCtx*)arg;
    u64 expect    = SPSC_DATA_SEED;
    u64 chunk_rng = ctx->chunk_seed;
    u8  buf[SPSC_MAX_CHUNK];

    ctx->ok = true;

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
            if (!v.size) { test_thread_yield(); continue; }
            got  = v.data;
            held = v.size;
        }
        else
        {
            if (!ring_buffer_read(ctx->rb, buf, want)) { test_thread_yield(); continue; }
            got = buf;
        }

        for (u64 i = 0; i < want; ++i)
        {
            u8 w = (u8)xorshift64(&expect);
            if (got[i] != w && ctx->ok)   /* record only the first mismatch */
            {
                ctx->ok        = false;
                ctx->fail_pos  = ctx->moved + i;
                ctx->fail_got  = got[i];
                ctx->fail_want = w;
            }
        }

        /* verify BEFORE advancing: the view dies at advance_read */
        if (held) ring_buffer_advance_read(ctx->rb, held);

        ctx->moved += want;
    }
    THREAD_RETURN();
}

static void test_spsc_stress(void)
{
    SECTION("ring: SPSC stress -- two threads, random chunks, byte-exact PRNG stream");

    RingBuffer rb = ring_buffer_alloc(KB(64));

    SpscCtx p = {0};
    SpscCtx c = {0};
    p.rb = &rb;  p.total = rb.size * 512 + 123;  p.chunk_seed = 0xBADC0FFEE0DDF00Dull;
    c.rb = &rb;  c.total = p.total;              c.chunk_seed = 0x0123456789ABCDEFull;

    test_thread tc = test_thread_start(spsc_consumer, &c);
    test_thread tp = test_thread_start(spsc_producer, &p);
    test_thread_join(tp);
    test_thread_join(tc);

    ASSERT(p.moved == p.total);
    ASSERT(c.moved == c.total);
    ASSERT(c.ok);
    if (!c.ok)
        fprintf(stderr, "  first mismatch at byte %llu: got 0x%02X want 0x%02X\n",
                (unsigned long long)c.fail_pos, c.fail_got, c.fail_want);

    ring_buffer_release(&rb);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"mirror",          test_mirror},
    {"alloc",           test_alloc},
    {"roundtrip",       test_roundtrip},
    {"seam",            test_seam},
    {"peek_no_consume", test_peek_no_consume},
    {"available_advance", test_available_advance},
    {"guards",          test_guards},
    {"stream",          test_stream},
    {"counter_wrap",    test_counter_wrap},
    {"spsc_stress",     test_spsc_stress},
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
