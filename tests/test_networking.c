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

/* --- address parsing ----------------------------------------------------*/

static void test_parse_hostport(void)
{
    SECTION("iris: net_addr_parse_hostport -- ip, ip:port, localhost, and rejects");

    NetAddr addr = {0};
    ASSERT(net_addr_parse_hostport(STR("192.168.0.1"), 80, &addr));
    ASSERT(addr.ip[0] == 192 && addr.ip[1] == 168 && addr.ip[2] == 0 && addr.ip[3] == 1);
    ASSERT(addr.port == 80);   /* no ":" in the string -> falls back to default_port */

    NetAddr addr_with_port = {0};
    ASSERT(net_addr_parse_hostport(STR("192.168.0.1:23"), 80, &addr_with_port));
    ASSERT(addr_with_port.ip[3] == 1);
    ASSERT(addr_with_port.port == 23);  /* string port overrides default_port */

    NetAddr local = {0};
    ASSERT(net_addr_parse_hostport(STR("localhost"), 8080, &local));
    ASSERT(local.ip[0] == 127 && local.ip[1] == 0 && local.ip[2] == 0 && local.ip[3] == 1);
    ASSERT(local.port == 8080);

    NetAddr local_port = {0};
    ASSERT(net_addr_parse_hostport(STR("LOCALHOST:9000"), 0, &local_port));  /* case-insensitive */
    ASSERT(local_port.ip[0] == 127 && local_port.ip[3] == 1);
    ASSERT(local_port.port == 9000);

    NetAddr out = {0};
    ASSERT(!net_addr_parse_hostport(STR("192.168.0.1:99999"), 80, &out));  /* port exceeds u16 */
    ASSERT(!net_addr_parse_hostport(STR("192.168.0.1:abc"), 80, &out));    /* non-numeric port */
    ASSERT(!net_addr_parse_hostport(STR("not.an.ip"), 80, &out));          /* bad octets, no localhost match */
}

static void test_parse_hostport_trailing_colon_is_rejected(void)
{
    SECTION("iris: net_addr_parse_hostport -- trailing ':' with no port is malformed, not a default-port shorthand");

    /* Unlike a URL parser (RFC 3986 lets an empty port after ':' mean "use the
     * scheme default"), this is a low-level address parser with no scheme/default
     * to fall back on -- matches net_addr_parse's existing strict-empty-field
     * rejection for octets, and str8_to_u64's rejection of empty input. */
    NetAddr out = {0};
    ASSERT(!net_addr_parse_hostport(STR("192.168.0.1:"), 80, &out));
    ASSERT(!net_addr_parse_hostport(STR("localhost:"), 80, &out));
}

/* --- address formatting -------------------------------------------------*/

static void test_addr_to_cstr_and_str8(void)
{
    SECTION("iris: net_addr_to_cstr / net_addr_to_str8 -- formats and truncates safely");

    NetAddr addr = net_addr(192, 168, 0, 1, 8080);

    char full[NET_ADDR_STR_MAX];
    u32 full_len = net_addr_to_cstr(addr, full, sizeof(full));
    ASSERT(full_len == (u32)strlen(full));
    ASSERT(strcmp(full, "192.168.0.1:8080") == 0);

    char tiny[6];
    u32 want_len = net_addr_to_cstr(addr, tiny, sizeof(tiny));
    ASSERT(want_len == full_len);              /* snprintf reports the untruncated length */
    ASSERT(strlen(tiny) == sizeof(tiny) - 1);  /* but the buffer itself is safely truncated + NUL-terminated */

    Arena* arena = arena_alloc(KB(4));
    str8 s = net_addr_to_str8(arena, addr);
    ASSERT(str8_eq(view_from_str8(s), STR("192.168.0.1:8080")));
    arena_release(arena);
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
    {"context",                       test_context},
    {"parse_hostport",                test_parse_hostport},
    {"parse_hostport_trailing_colon", test_parse_hostport_trailing_colon_is_rejected},
    {"addr_to_cstr_and_str8",         test_addr_to_cstr_and_str8},
    {"tcp_roundtrip",                 test_tcp_roundtrip},
    {"udp_roundtrip",                 test_udp_roundtrip},
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
