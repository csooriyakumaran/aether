#define _CRT_SECURE_NO_WARNINGS /* getenv() below */
#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#define HERMES_IMPLEMENTATION
#include "hermes/hermes.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* This is the one TU that compiles the hermes (and aether) implementation --
   HERMES_IMPLEMENTATION above pulls in real bodies for
   serial_open/serial_close/serial_valid/serial_read/serial_write. */

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

/* --- context cracking ------------------------------------------------------*/

static void test_context(void)
{
    SECTION("hermes: context cracking -- exactly one compiler/os/arch/lang detected");

    ASSERT(HERMES_COMPILER_MSVC + HERMES_COMPILER_GCC + HERMES_COMPILER_CLANG == 1);
    ASSERT(HERMES_OS_WINDOWS + HERMES_OS_MAC + HERMES_OS_LINUX + HERMES_OS_BSD == 1);
    ASSERT(HERMES_ARCH_X64 + HERMES_ARCH_ARM64 + HERMES_ARCH_X86 == 1);
    ASSERT(HERMES_LANG_C + HERMES_LANG_CPP == 1);

    ASSERT(HERMES_OS_WINDOWS);   /* the only supported OS today; widen with the port */
}

/* --- virtual com0com handshake ---------------------------------------------*/

/* Requires a null-modem virtual pair (e.g. com0com) already set up, wired
   COM10 <-> COM11 by default -- override with the HERMES_TEST_PORT_A /
   HERMES_TEST_PORT_B env vars if your pair uses different names. Not
   registered as a ctest case (same rationale as CONSOLE_SIGNAL_TEST_CASES'
   fires_on_ctrl_c in tests/CMakeLists.txt: needs manual setup ctest can't
   provide) -- run by hand:
       test_serial.exe handshake
*/
static str8 test_port_(const char* env_name, const char* fallback)
{
    const char* v = getenv(env_name);
    return str8_from_c_str(v ? v : fallback);
}

static void test_handshake(void)
{
    SECTION("hermes: serial -- two-way handshake over a virtual null-modem pair");

    str8 port_a = test_port_("HERMES_TEST_PORT_A", "COM10");
    str8 port_b = test_port_("HERMES_TEST_PORT_B", "COM11");

    /* SerialParity_None/SerialStopBits_One/SerialMode_RS232 all default on
       zero-fill. timeout_mode is explicit here, though -- SerialTimeoutMode_Block
       (the zero-fill default) would wait for the full sizeof(buf) bytes, which
       never arrives since PING/PONG are 4 bytes each. */
    SerialConfig cfg = {.baud = 9600, .timeout_mode = SerialTimeoutMode_Timeout, .timeout_ms = 1000};

    SerialPort a = serial_open(port_a, cfg);
    SerialPort b = serial_open(port_b, cfg);

    b8 a_ok = serial_valid(a);
    b8 b_ok = serial_valid(b);
    ASSERT(a_ok);
    ASSERT(b_ok);
    if (!a_ok || !b_ok)
    {
        fprintf(stderr, "  (set up a virtual null-modem pair first, e.g. com0com COM10<->COM11)\n");
        serial_close(&a);
        serial_close(&b);
        return;
    }

    const char* ping     = "PING";
    u64         ping_len = (u64)strlen(ping);

    u64          sent = 0;
    SerialResult r    = serial_write(a, ping, ping_len, &sent);
    ASSERT(r == SerialResult_OK);
    ASSERT(sent == ping_len);
    if (r != SerialResult_OK)   /* serial_read below has a timeout, so it won't hang --
                                    but there's nothing useful to check if this failed */
    {
        serial_close(&a);
        serial_close(&b);
        return;
    }

    char buf[64] = {0};
    u64  got     = 0;
    r = serial_read(b, buf, sizeof(buf), &got);
    ASSERT(r == SerialResult_OK);
    ASSERT(got == ping_len);
    ASSERT(memcmp(buf, ping, got) == 0);

    const char* pong     = "PONG";
    u64         pong_len = (u64)strlen(pong);

    sent = 0;
    r = serial_write(b, pong, pong_len, &sent);
    ASSERT(r == SerialResult_OK);
    ASSERT(sent == pong_len);
    if (r != SerialResult_OK)
    {
        serial_close(&a);
        serial_close(&b);
        return;
    }

    memset(buf, 0, sizeof(buf));
    got = 0;
    r = serial_read(a, buf, sizeof(buf), &got);
    ASSERT(r == SerialResult_OK);
    ASSERT(got == pong_len);
    ASSERT(memcmp(buf, pong, got) == 0);

    serial_close(&a);
    serial_close(&b);
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"context",   test_context},
    {"handshake", test_handshake},
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
