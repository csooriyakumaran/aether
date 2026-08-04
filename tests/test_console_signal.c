#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#include "aether/aether.h"

#include <stdio.h>
#include <string.h>

#if AETHER_OS_WINDOWS
#include <windows.h>
#endif

/* console_signal_install/_uninstall, validated against docs/console-signal.md's
   Validation plan. Two cases: the state-machine contract (no real OS event,
   always safe), and firing a genuine CTRL_C_EVENT (reaches around the public
   API into raw Win32 -- the design doc's own flagged CI-unfriendly case,
   because GenerateConsoleCtrlEvent broadcasts to the whole console process
   group by default). Two mitigations below, both required:

   1. FreeConsole()+AllocConsole() before firing: makes this process the sole
      member of a fresh console, so group 0 reaches only it, not whatever
      launched it (ctest, a parent shell).
   2. SetConsoleCtrlHandler(NULL, TRUE): disables the OS default action for
      CTRL_C_EVENT (process termination) for the whole test. Independent of
      our own handler registration -- a real handler installed via
      console_signal_install still fires normally; this only removes the
      "nothing is listening" fallback, which matters here specifically
      because the uninstall check fires an event while nothing IS listening. */

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

/* --- install/uninstall contract, no real console event --------------------- */

static void noop_handler(void* user) { (void)user; }

static void test_install_contract(void)
{
    SECTION("console_signal: install/uninstall contract");

    ASSERT(!console_signal_install(NULL, NULL));            /* fn required */

    int dummy = 0;
    ASSERT(console_signal_install(noop_handler, &dummy));
    ASSERT(!console_signal_install(noop_handler, &dummy));  /* single active handler only */

    console_signal_uninstall();
    console_signal_uninstall();                             /* idempotent when none installed */

    ASSERT(console_signal_install(noop_handler, &dummy));   /* gate reopens after a clean uninstall */
    console_signal_uninstall();
}

/* --- fires on a real console event ------------------------------------------ */

#if AETHER_OS_WINDOWS

typedef struct FireCtx { u64 count; void* seen_user; } FireCtx;
static FireCtx g_fire_ctx;

static void count_handler(void* user)
{
    /* runs on Windows' own ctrl-handler thread -- keep it tiny (design doc
       Pitfalls). Plain writes first, atomic release last: same publish
       order as console_signal_install itself (design doc decision 4), so
       wait_for_count's acquire load makes seen_user visible too. */
    g_fire_ctx.seen_user = user;
    atomic_store_rel_u64(&g_fire_ctx.count, g_fire_ctx.count + 1);
}

static b8 wait_for_count(u64 want, u32 timeout_ms)
{
    u64 start = time_mark();
    while (atomic_load_acq_u64(&g_fire_ctx.count) < want)
    {
        if (time_elapsed_sec(start, time_mark()) * 1000.0 > (f64)timeout_ms) return false;
        thread_sleep_ms(5);
    }
    return true;
}

static void test_fires_on_ctrl_c(void)
{
    SECTION("console_signal: CTRL_C actually invokes the handler (isolated console)");

    FreeConsole();
    ASSERT(AllocConsole());
    ASSERT(SetConsoleCtrlHandler(NULL, TRUE));   /* disable default termination, see file header */

    g_fire_ctx.count     = 0;
    g_fire_ctx.seen_user = NULL;
    int sentinel = 0xC0FFEE;

    ASSERT(console_signal_install(count_handler, &sentinel));
    ASSERT(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    ASSERT(wait_for_count(1, 2000));
    ASSERT(g_fire_ctx.seen_user == &sentinel);

    /* uninstalled: the same event no longer reaches the handler */
    console_signal_uninstall();
    u64 before = g_fire_ctx.count;
    ASSERT(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    thread_sleep_ms(100);   /* give a wrongly-still-installed handler a chance to fire */
    ASSERT(g_fire_ctx.count == before);

    SetConsoleCtrlHandler(NULL, FALSE);   /* restore default handling before exiting */
    FreeConsole();
}

#endif // AETHER_OS_WINDOWS

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"install_contract", test_install_contract},
#if AETHER_OS_WINDOWS
    {"fires_on_ctrl_c",  test_fires_on_ctrl_c},
#endif
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
