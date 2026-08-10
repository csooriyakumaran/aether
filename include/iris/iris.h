/*---------------------------------------------------------------------------*\
  IRIS

  Minimal Networking Library for C/C++

  Author      : C. Sooriyakumaran
  Created     : 2026-07-14
  License     : MIT

  https://github.com/csooriyakumaran/aether

  DESCRIPTIONS
  ------------

  Sockets, ... todo(chris)

  Do this:
      #define IRIS_IMPLEMENTATION
  before you include this file in *one* C or C++ file to create the implementation.

  // i.e.
  #include ...
  #include ...
  #include ...

  #define IRIS_IMPLEMENTATION
  #include "iris/iris.h"

  LINKAGE DEFINES
  --------------

  - IRIS_STATIC               API functions become static (private to the TU)
                              Requires IRIS_IMPLEMENTATION in the *same* file;
                              no other TU can use the library.
  - IRIS_BUILD_DLL            building iris as a shared library. Define together
                              with IRIS_IMPLEMENTATION in the DLL's TU; marks the
                              API dllexport (visibility("default") on POSIX)
  - IRIS_DLL                  consuming iris as a shared library; marks the API
                              dllimport. Do not define IRIS_IMPLEMENTATION.

  CONFIG DEFINES
  --------------

  - IRIS_BUILD_DEBUG=0|  1    force debug/release behaviour
                              (default: 1 unless NDEBUG is defined)
  - IRIS_ENABLE_ASSERTS=0|1   force asserts on/off
                              (default: IRIS_BUILD_DEBUG)

  OPTIONAL OPT-OUT DEFINES
  ------------------------
  todo(chris): tbd


\*---------------------------------------------------------------------------*/
#ifndef IRIS_H_
#define IRIS_H_

/*-------- C O N T E X T ----------------------------------------------------*/

// COMPILER
#if defined(__clang__)
    #define IRIS_COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define IRIS_COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define IRIS_COMPILER_GCC 1
#endif 

#if !defined(IRIS_COMPILER_MSVC)
    #define IRIS_COMPILER_MSVC 0
#endif
#if !defined(IRIS_COMPILER_GCC)
    #define IRIS_COMPILER_GCC 0
#endif
#if !defined(IRIS_COMPILER_CLANG)
    #define IRIS_COMPILER_CLANG 0
#endif

#if !(IRIS_COMPILER_MSVC || IRIS_COMPILER_CLANG || IRIS_COMPILER_GCC)
    #error "IRIS: unsupported compiler"
#endif

// OS
#if defined(_WIN32)
    #define IRIS_OS_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define IRIS_OS_MAC 1
    #endif
#elif defined(__linux__)
    #define IRIS_OS_LINUX 1
    #if defined(__ANDROID__)
        #define IRIS_OS_ANDROID 1
    #endif
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define IRIS_OS_BSD 1
#endif

#if !defined(IRIS_OS_WINDOWS)
    #define IRIS_OS_WINDOWS 0
#endif
#if !defined(IRIS_OS_MAC)
    #define IRIS_OS_MAC 0
#endif
#if !defined(IRIS_OS_LINUX)
    #define IRIS_OS_LINUX 0
#endif
#if !defined(IRIS_OS_ANDROID)
    #define IRIS_OS_ANDROID 0
#endif
#if !defined(IRIS_OS_BSD)
    #define IRIS_OS_BSD 0
#endif

#define IRIS_OS_POSIX (IRIS_OS_MAC || IRIS_OS_LINUX || IRIS_OS_BSD)

#if !(IRIS_OS_WINDOWS || IRIS_OS_LINUX || IRIS_OS_MAC || IRIS_OS_BSD)
    #error "IRIS: unsupported os"
#endif

// ARCH
#if IRIS_COMPILER_MSVC
    #if defined(_M_X64)
        #define IRIS_ARCH_X64 1
    #elif defined (_M_ARM64)
        #define IRIS_ARCH_ARM64 1
    #elif defined (_M_IX86)
        #define IRIS_ARCH_X86 1
    #endif
#elif IRIS_COMPILER_CLANG || IRIS_COMPILER_GCC
    #if defined(__x86_64__)
        #define IRIS_ARCH_X64 1
    #elif defined(__aarch64__)
        #define IRIS_ARCH_ARM64 1
    #elif defined(__i386__)
        #define IRIS_ARCH_X86 1
    #endif
#endif

#if !defined(IRIS_ARCH_X64)
    #define IRIS_ARCH_X64 0
#endif
#if !defined(IRIS_ARCH_ARM64)
    #define IRIS_ARCH_ARM64 0
#endif
#if !defined(IRIS_ARCH_X86)
    #define IRIS_ARCH_X86 0
#endif 

#if !(IRIS_ARCH_X64 || IRIS_ARCH_ARM64 || IRIS_ARCH_X86)
    #error "IRIS: unsupported architecture"
#endif

// LANG
#if defined(__cplusplus)
    #define IRIS_LANG_CPP 1
    #define IRIS_LANG_C 0
#else
    #define IRIS_LANG_C 1
    #define IRIS_LANG_CPP 0
#endif


// BUILD
#ifndef IRIS_BUILD_DEBUG
    #if !defined(NDEBUG)
        #define IRIS_BUILD_DEBUG 1
    #else
        #define IRIS_BUILD_DEBUG 0
    #endif
#endif // IRIS_BUILD_DEBUG

#if defined(IRIS_STATIC) && defined(IRIS_DLL)
#error "IRIS_STATIC and IRIS_DLL are mutually exclusive"
#endif

#if defined(IRIS_STATIC) && defined(IRIS_BUILD_DLL)
    #error "IRIS_STATIC and IRIS_BUILD_DLL are mutually exclusive"
#endif

#if defined(IRIS_IMPLEMENTATION) && defined(IRIS_DLL) && !defined(IRIS_BUILD_DLL)
    #error "Cannot compile the implementation in DLL-import mode; define IRIS_BUILD_DLL"
#endif

#if IRIS_OS_WINDOWS
    #define IRIS_DLL_EXPORT __declspec(dllexport)
    #define IRIS_DLL_IMPORT __declspec(dllimport)
#else
    #define IRIS_DLL_EXPORT __attribute__((visibility("default")))
    #define IRIS_DLL_IMPORT extern
#endif

#if defined(IRIS_BUILD_DLL)
    #define IRIS_API IRIS_DLL_EXPORT
#elif defined(IRIS_DLL)
    #define IRIS_API IRIS_DLL_IMPORT
#elif defined(IRIS_STATIC)
    #define IRIS_API static
#else
    #define IRIS_API extern
#endif


/*---------------------------------------------------------------------------*/

#if !IRIS_OS_WINDOWS
    #error "IRIS: Currently only supports Windows"
#endif 

/*---------------------------------------------------------------------------*/

#ifndef AETHER_H_
    #include "aether/aether.h"
#endif

#if IRIS_LANG_CPP
extern "C"
{
#endif // IRIS_LANG_CPP


/*-------- N E T W O R K I N G ----------------------------------------------*/

/* Process-wide socket layer lifetime (WSAStartup/WSACleanup on Windows;
   no-ops on POSIX). Call net_init once from main before any other iris
   call and before threads start; mirror with net_shutdown at exit.
   Not reference-counted, not thread-safe. */

IRIS_API b8   net_init(void);
IRIS_API void net_shutdown(void);

#define NET_ADDR_STR_MAX 22 /* 255.255.255.255:65535 + NUL */
typedef struct NetAddr { u8 ip[4]; u16 port; } NetAddr;      /* ip wire-order, port host-order */

IRIS_API NetAddr net_addr(u8 a, u8 b, u8 c, u8 d, u16 port); 
IRIS_API NetAddr net_addr_any(u16 port);                    /* 0.0.0.0 - all interfaces */
IRIS_API NetAddr net_addr_loopback(u16 port);               /* 127.0.0.1                */
IRIS_API b8      net_addr_parse(str8_view ip, u16 port, NetAddr* out); /* numeric only, no DNS */
IRIS_API b8      net_addr_parse_hostport(str8_view s, u16 default_port, NetAddr* out);
IRIS_API u32     net_addr_to_cstr(NetAddr addr, char *buf, u64 buf_size);
IRIS_API str8    net_addr_to_str8(Arena* arena, NetAddr addr);

typedef struct Socket { u64 handle; } Socket; /* {0} = invalid */

IRIS_API b8   socket_valid(Socket s); 
IRIS_API void socket_close(Socket* s); /* zeros the handle, cross-thread cancellation*/

typedef u8 NetResult;
enum NetResult_
{
    NetResult_OK = 0,
    NetResult_Closed,     /* orderly peer shutdown (tcp_recv only)   */
    NetResult_Error,      /* socket is dead; close it                */
};

/* TCP. All calls are blocking; TCP_NODELAY is always set. tcp_accept returns {0}
 * only on cancellation or a hard failure*/
IRIS_API Socket    tcp_listen(NetAddr addr, u32 backlog);              /* {0} on failure */
IRIS_API Socket    tcp_accept(Socket listener, NetAddr* out_peer);     /* {0} if none pending (or error) */
IRIS_API Socket    tcp_connect(NetAddr addr);                          /* {0} on failure */
IRIS_API NetResult tcp_send(Socket s, bytes_view data, u64* out_sent); /* partial sends are OK */
IRIS_API NetResult tcp_recv(Socket s, void* buf, u64 cap, u64* out_recv);

/* UDP. Datagrams send whole or not at all; oversized datagrams are 
 * NetResult_Error. udp_open with port 0 = ephemeral send-only socket */
IRIS_API Socket    udp_open(NetAddr bind_addr);
IRIS_API NetResult udp_send_to(Socket s, NetAddr to, bytes_view datagram);
IRIS_API NetResult udp_recv_from(Socket s, void* buf, u64 cap, u64* out_recv, NetAddr* out_from);

#if IRIS_LANG_CPP
}
#endif // IRIS_LANG_CPP


#endif // IRIS_H_

/*---------------------------------------------------------------------------*/

#if defined(IRIS_IMPLEMENTATION) && !defined(IRIS_IMPLEMENTATION_DONE)
#define IRIS_IMPLEMENTATION_DONE

/*---------------------------------------------------------------------------*/
/* --- P L A T F O R M ----------------------------------------------------- */
/*---------------------------------------------------------------------------*/
#if IRIS_OS_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif // WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif // NOMINMAX
    #include <windows.h>

    typedef int     (WSAAPI *WSAStartup_fn)(WORD, LPWSADATA);
    typedef int     (WSAAPI *WSACleanup_fn)(void);
    typedef SOCKET  (WSAAPI *socket_fn)(int, int, int);
    typedef int     (WSAAPI *setsockopt_fn)(SOCKET, int, int, const char*, int);
    typedef int     (WSAAPI *bind_fn)(SOCKET, const struct sockaddr*, int);
    typedef int     (WSAAPI *listen_fn)(SOCKET, int);
    typedef int     (WSAAPI *closesocket_fn)(SOCKET);
    typedef u_short (WSAAPI *htons_fn)(u_short);
    typedef u_short (WSAAPI *ntohs_fn)(u_short);
    typedef SOCKET  (WSAAPI *accept_fn)(SOCKET, struct sockaddr*, int*);
    typedef int     (WSAAPI *connect_fn)(SOCKET, const struct sockaddr*, int);
    typedef int     (WSAAPI *send_fn)(SOCKET, const char*, int, int);
    typedef int     (WSAAPI *recv_fn)(SOCKET, char*, int, int);
    typedef int     (WSAAPI *sendto_fn)(SOCKET, const char*, int, int, const struct sockaddr*, int);
    typedef int     (WSAAPI *recvfrom_fn)(SOCKET, char*, int, int, struct sockaddr*, int*);

    global   HMODULE os_ws2_dll_ = NULL;
    internal FARPROC os_ws2_sym_(const char* name)
    {
        return os_ws2_dll_ ? GetProcAddress(os_ws2_dll_, name) : NULL;
    }

    internal SOCKADDR_IN os_addr_to_sockaddr_(NetAddr addr)
    {
        htons_fn phtons = (htons_fn)os_ws2_sym_("htons");
        SOCKADDR_IN sa = {0};
        sa.sin_family = AF_INET;
        sa.sin_port   = phtons ? phtons(addr.port) : 0;
        memcpy(&sa.sin_addr, addr.ip, 4);
        return sa;
    }

    internal NetAddr os_sockaddr_to_addr_(SOCKADDR_IN sa)
    {
        ntohs_fn pntohs = (ntohs_fn)os_ws2_sym_("ntohs");
        NetAddr addr = {0};
        memcpy(addr.ip, &sa.sin_addr, 4);
        addr.port = pntohs ? pntohs(sa.sin_port) : 0;
        return addr;
    }

    internal u64 os_socket_from_raw_(SOCKET s) { return (s == INVALID_SOCKET) ? 0 : (u64)s + 1; }

#endif // IRIS_OS_WINDOWS

/*---------------------------------------------------------------------------*/

#if IRIS_LANG_CPP
extern "C"
{
#endif // IRIS_LANG_CPP

internal b8 os_net_init(void)
{
#if IRIS_OS_WINDOWS
    os_ws2_dll_ = LoadLibraryExW(L"ws2_32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!os_ws2_dll_) return false;

    WSAStartup_fn wsa_startup = (WSAStartup_fn)os_ws2_sym_("WSAStartup");
    WSADATA wsadata;
    if (!wsa_startup || wsa_startup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        FreeLibrary(os_ws2_dll_);
        os_ws2_dll_ = NULL;
        return false;
    }
    return true;
#else // IRIS_OS_POSIX
    return true;
#endif
}

internal void os_net_shutdown(void)
{
#if IRIS_OS_WINDOWS
    if (!os_ws2_dll_) return;
    WSACleanup_fn wsa_cleanup = (WSACleanup_fn)os_ws2_sym_("WSACleanup");
    if (wsa_cleanup) wsa_cleanup();
    FreeLibrary(os_ws2_dll_);
    os_ws2_dll_ = NULL;
#endif
}

internal void os_socket_close(u64 h)
{
#if IRIS_OS_WINDOWS
    closesocket_fn pclosesocket = (closesocket_fn)os_ws2_sym_("closesocket");
    if (!pclosesocket) return;
    pclosesocket((SOCKET)(h-1));
#endif
}

internal u64 os_tcp_listen(NetAddr addr, u32 backlog)
{
#if IRIS_OS_WINDOWS
    socket_fn     psocket     = (socket_fn)     os_ws2_sym_("socket");
    setsockopt_fn psetsockopt = (setsockopt_fn) os_ws2_sym_("setsockopt");
    bind_fn       pbind       = (bind_fn)       os_ws2_sym_("bind");
    listen_fn     plisten     = (listen_fn)     os_ws2_sym_("listen");

    if (!psocket || !psetsockopt || !pbind || !plisten) return 0;

    SOCKET s = psocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    BOOL exclusive = TRUE;
    psetsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive)); 

    SOCKADDR_IN sa = os_addr_to_sockaddr_(addr);
    if (pbind(s, (SOCKADDR*)&sa, sizeof(sa)) == SOCKET_ERROR)
    {
        os_socket_close(os_socket_from_raw_(s));
        return 0;
    }

    if (plisten(s, (int)backlog) == SOCKET_ERROR)
    {
        os_socket_close(os_socket_from_raw_(s));
        return 0;
    }

    return os_socket_from_raw_(s);
#else // IRIS_OS_POSIX
    #error "IRIS: OS tcp listen not implemented on this platform"
#endif
}

internal u64 os_tcp_accept(u64 h, NetAddr* out_peer)
{
#if IRIS_OS_WINDOWS
    if (!h) return 0;

    accept_fn     paccept     = (accept_fn)     os_ws2_sym_("accept");
    setsockopt_fn psetsockopt = (setsockopt_fn) os_ws2_sym_("setsockopt");

    if (!paccept || !psetsockopt) return 0;

    SOCKET      listener = (SOCKET)(h-1);
    SOCKADDR_IN sa       = {0};
    int         addrlen  = sizeof(sa);

    SOCKET s = paccept(listener, (SOCKADDR*)&sa, &addrlen);
    if (s == INVALID_SOCKET) return 0;

    BOOL nodelay = TRUE;
    psetsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

    if (out_peer) *out_peer = os_sockaddr_to_addr_(sa);
    return os_socket_from_raw_(s);
#else // IRIS_OS_POSIX
    #error "IRIS: OS tcp accept not implemented on this platform"
#endif
}

internal u64 os_tcp_connect(NetAddr addr)
{
#if IRIS_OS_WINDOWS
    socket_fn     psocket     = (socket_fn)    os_ws2_sym_("socket");
    connect_fn    pconnect    = (connect_fn)   os_ws2_sym_("connect");
    setsockopt_fn psetsockopt = (setsockopt_fn)os_ws2_sym_("setsockopt");

    if (!psocket || !pconnect || !psetsockopt) return 0;

    SOCKET s = psocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;

    SOCKADDR_IN sa = os_addr_to_sockaddr_(addr);
    if (pconnect(s, (SOCKADDR*)&sa, sizeof(sa)) == SOCKET_ERROR)
    {
        os_socket_close(os_socket_from_raw_(s));
        return 0;
    }

    BOOL nodelay = TRUE;
    psetsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
    return os_socket_from_raw_(s);
#else // IRIS_OS_POSIX
    #error "IRIS: OS tcp connect not implemented on this platform"
#endif
}

internal NetResult os_tcp_send(u64 h, const u8* data, u64 len, u64* out_sent)
{
#if IRIS_OS_WINDOWS
    if (out_sent) *out_sent = 0;
    if (!h) return NetResult_Error;

    send_fn psend = (send_fn)os_ws2_sym_("send");
    if (!psend) return NetResult_Error;

    SOCKET s     = (SOCKET)(h - 1);
    int    chunk = (len > (u64)INT_MAX) ? INT_MAX : (int)len;

    int sent = psend(s, (const char*)data, chunk, 0);
    if (sent == SOCKET_ERROR) return NetResult_Error;

    if (out_sent) *out_sent = (u64)sent;
    return NetResult_OK;
#else // IRIS_OS_POSIX
    #error "IRIS: OS tcp send not implemented on this platform"
#endif
}

internal NetResult os_tcp_recv(u64 h, u8* buf, u64 cap, u64* out_recv)
{
#if IRIS_OS_WINDOWS
    if (out_recv) *out_recv = 0;
    if (!h) return NetResult_Error;

    recv_fn precv = (recv_fn)os_ws2_sym_("recv");
    if (!precv) return NetResult_Error;

    SOCKET s     = (SOCKET)(h - 1);
    int    chunk = (cap > (u64)INT_MAX) ? INT_MAX : (int)cap;

    int got = precv(s, (char*)buf, chunk, 0);
    if (got == 0)            return NetResult_Closed;
    if (got == SOCKET_ERROR) return NetResult_Error;

    if (out_recv) *out_recv = (u64)got;
    return NetResult_OK;
#else // IRIS_OS_POSIX
    #error "IRIS: OS tcp recv not implemented on this platform"
#endif
}

internal u64 os_udp_open(NetAddr bind_addr)
{
#if IRIS_OS_WINDOWS
    socket_fn psocket = (socket_fn)os_ws2_sym_("socket");
    bind_fn   pbind   = (bind_fn)  os_ws2_sym_("bind");
    if (!psocket || !pbind) return 0;

    SOCKET s = psocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return 0;

    SOCKADDR_IN sa = os_addr_to_sockaddr_(bind_addr);
    if (pbind(s, (SOCKADDR*)&sa, sizeof(sa)) == SOCKET_ERROR)
    {
        os_socket_close(os_socket_from_raw_(s));
        return 0;
    }

    return os_socket_from_raw_(s);
#else // IRIS_OS_POSIX
    #error "IRIS: OS udp open not implemented on this platform"
#endif
}

internal NetResult os_udp_send_to(u64 h, NetAddr to, const u8* data, u64 len)
{
#if IRIS_OS_WINDOWS
    if (!h) return NetResult_Error;
    if (len > (u64)INT_MAX) return NetResult_Error; /* oversized datagram */

    sendto_fn psendto = (sendto_fn)os_ws2_sym_("sendto");
    if (!psendto) return NetResult_Error;

    SOCKET      s  = (SOCKET)(h - 1);
    SOCKADDR_IN sa = os_addr_to_sockaddr_(to);

    int sent = psendto(s, (const char*)data, (int)len, 0, (SOCKADDR*)&sa, sizeof(sa));
    if (sent == SOCKET_ERROR || (u64)sent != len) return NetResult_Error;

    return NetResult_OK;
#else // IRIS_OS_POSIX
    #error "IRIS: OS udp send to not implemented on this platform"
#endif
}

internal NetResult os_udp_recv_from(u64 h, u8* buf, u64 cap, u64* out_recv, NetAddr* out_from)
{
#if IRIS_OS_WINDOWS
    if (out_recv) *out_recv = 0;
    if (!h) return NetResult_Error;

    recvfrom_fn precvfrom = (recvfrom_fn)os_ws2_sym_("recvfrom");
    if (!precvfrom) return NetResult_Error;

    SOCKET      s       = (SOCKET)(h - 1);
    int         chunk   = (cap > (u64)INT_MAX) ? INT_MAX : (int)cap;
    SOCKADDR_IN sa      = {0};
    int         addrlen = sizeof(sa);

    int got = precvfrom(s, (char*)buf, chunk, 0, (SOCKADDR*)&sa, &addrlen);
    if (got == SOCKET_ERROR) return NetResult_Error;

    if (out_from) *out_from = os_sockaddr_to_addr_(sa);
    if (out_recv) *out_recv = (u64)got;
    return NetResult_OK;
#else // IRIS_OS_POSIX
    #error "IRIS: OS udp recv from not implemented on this platform"
#endif
}


/* ------------------------------------------------------------------------- */
/* --- N E T W O R K ------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
IRIS_API b8 net_init(void)
{
    return os_net_init();
}

IRIS_API void net_shutdown(void)
{
    os_net_shutdown();
}

IRIS_API NetAddr net_addr(u8 a, u8 b, u8 c, u8 d, u16 port)
{
    NetAddr addr;
    addr.ip[0] = a; addr.ip[1] = b; addr.ip[2] = c; addr.ip[3] = d;
    addr.port  = port;
    return addr;
}

IRIS_API NetAddr net_addr_any(u16 port)      { return net_addr(0, 0, 0, 0, port); }
IRIS_API NetAddr net_addr_loopback(u16 port) { return net_addr(127, 0, 0, 1, port); }

/* str8_to_u64 also accepts 0x/0o/0b literals (aether's general integer syntax) --
 * that's not the surface net_addr's "numeric only" contract means to expose, so
 * octets/ports are pre-validated as plain decimal digits before conversion. */
internal b8 str8_is_decimal_(str8_view s)
{
    if (s.size == 0) return false;

    u64 i = (s.data[0] == '+') ? 1 : 0;
    if (i >= s.size) return false;   /* sign with no digits */

    for (; i < s.size; ++i)
        if (s.data[i] < '0' || s.data[i] > '9') return false;

    return true;
}

IRIS_API b8 net_addr_parse(str8_view ip, u16 port, NetAddr* out)
{
    if (!out) return false;

    u8 octets[4];
    str8_view rest = ip;

    for (int i = 0; i < 4; i++)
    {
        str8_view field;
        if (i < 3)
        {
            str8_view after;
            if (!str8_cut(rest, STR("."), &field, &after)) return false;
            rest = after;
        }
        else
        {
            field = rest;
        }

        u64 v;
        if (field.size == 0 || field.size > 3) return false;
        if (!str8_is_decimal_(field)) return false;
        if (!str8_to_u64(field, &v) || v > 255) return false;
        octets[i] = (u8)v;
    }

    *out = net_addr(octets[0], octets[1], octets[2], octets[3], port);
    return true;
}

IRIS_API b8 net_addr_parse_hostport(str8_view s, u16 default_port, NetAddr* out)
{
    if (!out) return false;

    str8_view host = s;
    u16       port = default_port;

    str8_view before, after;
    if (str8_cut(s, STR(":"), &before, &after))
    {
        u64 v;
        if (after.size == 0 || after.size > 5) return false;
        if (!str8_is_decimal_(after)) return false;
        if (!str8_to_u64(after, &v) || v > AETHER_U16_MAX_) return false;
        host = before;
        port = (u16)v;
    }

    if (str8_eq_nocase(host, STR("localhost")))
    {
        *out = net_addr_loopback(port);
        return true;
    }

    return net_addr_parse(host, port, out);
}

IRIS_API u32 net_addr_to_cstr(NetAddr addr, char* buf, u64 buf_size)
{
    int n = snprintf(buf, buf_size, "%u.%u.%u.%u:%u",
                     addr.ip[0], addr.ip[1], addr.ip[2], addr.ip[3], addr.port);

    return (u32)n;
}

IRIS_API str8 net_addr_to_str8(Arena* arena, NetAddr addr)
{
    char buf[NET_ADDR_STR_MAX];
    net_addr_to_cstr(addr, buf, sizeof(buf));
    return str8_push_c_str(arena, buf);
}

IRIS_API b8 socket_valid(Socket s)
{
    return s.handle != 0;
}

IRIS_API void socket_close(Socket* s) /* zeros the handle */
{
    if (!s || !s->handle) return;
    os_socket_close(s->handle);
    s->handle = 0;
}

/* ------------------------------------------------------------------------- */
/* --- T C P --------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

IRIS_API Socket tcp_listen(NetAddr addr, u32 backlog)
{
    Socket s = {0};
    s.handle = os_tcp_listen(addr, backlog);
    return s;
}

IRIS_API Socket tcp_accept(Socket listener, NetAddr* out_peer)
{
    Socket s = {0};
    s.handle = os_tcp_accept(listener.handle, out_peer);
    return s;
}

IRIS_API Socket tcp_connect(NetAddr addr)
{
    Socket s = {0};
    s.handle = os_tcp_connect(addr);
    return s;
}

IRIS_API NetResult tcp_send(Socket s, bytes_view data, u64* out_sent)
{
    return os_tcp_send(s.handle, data.data, data.size, out_sent);
}

IRIS_API NetResult tcp_recv(Socket s, void* buf, u64 cap, u64* out_recv)
{
    return os_tcp_recv(s.handle, (u8*)buf, cap, out_recv);
}

/* ------------------------------------------------------------------------- */
/* --- U D P --------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

IRIS_API Socket udp_open(NetAddr bind_addr)
{
    Socket s = {0};
    s.handle = os_udp_open(bind_addr);
    return s;
}

IRIS_API NetResult udp_send_to(Socket s, NetAddr to, bytes_view datagram)
{
    return os_udp_send_to(s.handle, to, datagram.data, datagram.size);
}

IRIS_API NetResult udp_recv_from(Socket s, void* buf, u64 cap, u64* out_recv, NetAddr* out_from)
{
    return os_udp_recv_from(s.handle, (u8*)buf, cap, out_recv, out_from);
}

#if IRIS_LANG_CPP
}
#endif // IRIS_LANG_CPP

#endif // IRIS_IMPLEMENTATION

/*---------------------------------------------------------------------------*\
   LICENSE
   -------

   Copyright 2026 C. Sooriyakumaran

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the “Software”), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell co-
   pies of the Software, and to permit persons to whom the Software is furnished
   to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
   NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUT-
   HORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

\*---------------------------------------------------------------------------*/
