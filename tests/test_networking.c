#define AETHER_NO_ASSERT
#define AETHER_IMPLEMENTATION
#define IRIS_IMPLEMENTATION
#include "iris/iris.h"

#include <stdio.h>
#include <string.h>

/* Skeleton for the iris networking tests.

   This is the one TU that compiles the iris implementation (and, through
   iris.h's include of aether/aether.h, the aether implementation) -- which
   also keeps iris.h reachable from compile_commands.json so clangd can infer
   flags for the header. Real socket/server/client cases land here as the
   API grows; until then the context case pins down the platform-detection
   layer the rest of iris will dispatch on. */

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
    SECTION("iris: context cracking -- exactly one compiler/os/arch/lang detected");

    ASSERT(IRIS_COMPILER_MSVC + IRIS_COMPILER_GCC + IRIS_COMPILER_CLANG == 1);
    ASSERT(IRIS_OS_WINDOWS + IRIS_OS_MAC + IRIS_OS_LINUX + IRIS_OS_BSD == 1);
    ASSERT(IRIS_ARCH_X64 + IRIS_ARCH_ARM64 + IRIS_ARCH_X86 == 1);
    ASSERT(IRIS_LANG_C + IRIS_LANG_CPP == 1);

    ASSERT(IRIS_OS_WINDOWS);   /* the only supported OS today; widen with the port */
}

/* --- tcp round trip ---------------------------------------------------- */

static void test_tcp_roundtrip(void)
{
    SECTION("iris: tcp -- listen/accept/connect + send/recv round trip");

    ASSERT(net_init());

    NetAddr addr = net_addr_loopback(54345);

    Socket listener = tcp_listen(addr, 1);
    ASSERT(socket_valid(listener));

    Socket client = tcp_connect(addr);
    ASSERT(socket_valid(client));

    NetAddr peer = {0};
    Socket server = tcp_accept(listener, &peer);
    ASSERT(socket_valid(server));
    ASSERT(peer.ip[0] == 127 && peer.ip[1] == 0 && peer.ip[2] == 0 && peer.ip[3] == 1);

    const char* msg = "hello iris";
    u64 msg_len = (u64)strlen(msg);

    u64 sent = 0;
    NetResult r = tcp_send(client, view_from_raw(msg, msg_len), &sent);
    ASSERT(r == NetResult_OK);
    ASSERT(sent == msg_len);

    char buf[64] = {0};
    u64 got = 0;
    r = tcp_recv(server, buf, sizeof(buf), &got);
    ASSERT(r == NetResult_OK);
    ASSERT(got == msg_len);
    ASSERT(memcmp(buf, msg, got) == 0);

    socket_close(&client);
    socket_close(&server);
    socket_close(&listener);
    net_shutdown();
}

/* --- udp round trip ---------------------------------------------------- */

static void test_udp_roundtrip(void)
{
    SECTION("iris: udp -- open/send_to/recv_from round trip");

    ASSERT(net_init());

    NetAddr addr_a = net_addr_loopback(54346);
    NetAddr addr_b = net_addr_loopback(54347);

    Socket sock_a = udp_open(addr_a);
    ASSERT(socket_valid(sock_a));

    Socket sock_b = udp_open(addr_b);
    ASSERT(socket_valid(sock_b));

    const char* msg = "hello udp";
    u64 msg_len = (u64)strlen(msg);

    NetResult r = udp_send_to(sock_a, addr_b, view_from_raw(msg, msg_len));
    ASSERT(r == NetResult_OK);

    char buf[64] = {0};
    u64 got = 0;
    NetAddr from = {0};
    r = udp_recv_from(sock_b, buf, sizeof(buf), &got, &from);
    ASSERT(r == NetResult_OK);
    ASSERT(got == msg_len);
    ASSERT(memcmp(buf, msg, got) == 0);
    ASSERT(from.ip[0] == 127 && from.ip[1] == 0 && from.ip[2] == 0 && from.ip[3] == 1);
    ASSERT(from.port == addr_a.port);

    socket_close(&sock_a);
    socket_close(&sock_b);
    net_shutdown();
}

typedef struct { const char* name; void (*fn)(void); } TestCase;
static TestCase g_cases[] = {
    {"context",        test_context},
    {"tcp_roundtrip",  test_tcp_roundtrip},
    {"udp_roundtrip",  test_udp_roundtrip},
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
