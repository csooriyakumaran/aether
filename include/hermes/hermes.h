/*---------------------------------------------------------------------------*\
  HERMES

  Minimal Serial Communication Library for C/C++

  Author      : C. Sooriyakumaran
  Created     : 2026-09-26
  License     : MIT

  https://github.com/csooriyakumaran/aether

  DESCRIPTIONS
  ------------

  Low-level RS232/RS485 serial port I/O: open/close a port, send bytes,
  read bytes. Framing, encoding, and message semantics are left entirely
  to caller code -- hermes only owns the OS/hardware boundary (port config,
  timeouts, RS485 TX/RX turnaround).

  Do this:
      #define AETHER_IMPLEMENTATION
      #define HERMES_IMPLEMENTATION
  before you include this file in *one* C or C++ file to create the implementation.

  // i.e.
  #include ...
  #include ...
  #include ...

  #define AETHER_IMPLEMENTATION
  #define HERMES_IMPLEMENTATION
  #include "hermes/hermes.h"

  LINKAGE DEFINES
  --------------

  - HERMES_STATIC             API functions become static (private to the TU)
                              Requires HERMES_IMPLEMENTATION in the *same* file;
                              no other TU can use the library.

  - HERMES_BUILD_DLL          building iris as a shared library. Define together
                              with HERMES_IMPLEMENTATION in the DLL's TU; marks
                              the API dllexport (visibility("default") on POSIX)

  - HERMES_DLL                consuming iris as a shared library; marks the API
                              dllimport. Do not define HERMES_IMPLEMENTATION.

  CONFIG DEFINES
  --------------

  - HERMES_BUILD_DEBUG=0|1    force debug/release behaviour
                              (default: 1 unless NDEBUG is defined)
  - HERMES_ENABLE_ASSERTS=0|1 force asserts on/off
                              (default: HERMES_BUILD_DEBUG)

  OPTIONAL OPT-OUT DEFINES
  ------------------------
  todo(chris): tbd

\*---------------------------------------------------------------------------*/
#ifndef HERMES_H_
#define HERMES_H_

/*-------- C O N T E X T ----------------------------------------------------*/

// COMPILER
#if defined(__clang__)
    #define HERMES_COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define HERMES_COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define HERMES_COMPILER_GCC 1
#endif

#if !defined(HERMES_COMPILER_MSVC)
    #define HERMES_COMPILER_MSVC 0
#endif
#if !defined(HERMES_COMPILER_GCC)
    #define HERMES_COMPILER_GCC 0
#endif
#if !defined(HERMES_COMPILER_CLANG)
    #define HERMES_COMPILER_CLANG 0
#endif

#if !(HERMES_COMPILER_MSVC || HERMES_COMPILER_CLANG || HERMES_COMPILER_GCC)
    #error "HERMES: unsupported compiler"
#endif

// OS
#if defined(_WIN32)
    #define HERMES_OS_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define HERMES_OS_MAC 1
    #endif
#elif defined(__linux__)
    #define HERMES_OS_LINUX 1
    #if defined(__ANDROID__)
        #define HERMES_OS_ANDROID 1
    #endif
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define HERMES_OS_BSD 1
#endif

#if !defined(HERMES_OS_WINDOWS)
    #define HERMES_OS_WINDOWS 0
#endif
#if !defined(HERMES_OS_MAC)
    #define HERMES_OS_MAC 0
#endif
#if !defined(HERMES_OS_LINUX)
    #define HERMES_OS_LINUX 0
#endif
#if !defined(HERMES_OS_ANDROID)
    #define HERMES_OS_ANDROID 0
#endif
#if !defined(HERMES_OS_BSD)
    #define HERMES_OS_BSD 0
#endif

#define HERMES_OS_POSIX (HERMES_OS_MAC || HERMES_OS_LINUX || HERMES_OS_BSD)

#if !(HERMES_OS_WINDOWS || HERMES_OS_LINUX || HERMES_OS_MAC || HERMES_OS_BSD)
    #error "HERMES: unsupported os"
#endif

// ARCH
#if HERMES_COMPILER_MSVC
    #if defined(_M_X64)
        #define HERMES_ARCH_X64 1
    #elif defined (_M_ARM64)
        #define HERMES_ARCH_ARM64 1
    #elif defined (_M_IX86)
        #define HERMES_ARCH_X86 1
    #endif
#elif HERMES_COMPILER_CLANG || HERMES_COMPILER_GCC
    #if defined(__x86_64__)
        #define HERMES_ARCH_X64 1
    #elif defined(__aarch64__)
        #define HERMES_ARCH_ARM64 1
    #elif defined(__i386__)
        #define HERMES_ARCH_X86 1
    #endif
#endif

#if !defined(HERMES_ARCH_X64)
    #define HERMES_ARCH_X64 0
#endif
#if !defined(HERMES_ARCH_ARM64)
    #define HERMES_ARCH_ARM64 0
#endif
#if !defined(HERMES_ARCH_X86)
    #define HERMES_ARCH_X86 0
#endif

#if !(HERMES_ARCH_X64 || HERMES_ARCH_ARM64 || HERMES_ARCH_X86)
    #error "HERMES: unsupported architecture"
#endif

// LANG
#if defined(__cplusplus)
    #define HERMES_LANG_CPP 1
    #define HERMES_LANG_C 0
#else
    #define HERMES_LANG_C 1
    #define HERMES_LANG_CPP 0
#endif


// BUILD
#ifndef HERMES_BUILD_DEBUG
    #if !defined(NDEBUG)
        #define HERMES_BUILD_DEBUG 1
    #else
        #define HERMES_BUILD_DEBUG 0
    #endif
#endif // HERMES_BUILD_DEBUG

#if defined(HERMES_STATIC) && defined(HERMES_DLL)
#error "HERMES_STATIC and HERMES_DLL are mutually exclusive"
#endif

#if defined(HERMES_STATIC) && defined(HERMES_BUILD_DLL)
    #error "HERMES_STATIC and HERMES_BUILD_DLL are mutually exclusive"
#endif

#if defined(HERMES_IMPLEMENTATION) && defined(HERMES_DLL) && !defined(HERMES_BUILD_DLL)
    #error "Cannot compile the implementation in DLL-import mode; define HERMES_BUILD_DLL"
#endif

#if HERMES_OS_WINDOWS
    #define HERMES_DLL_EXPORT __declspec(dllexport)
    #define HERMES_DLL_IMPORT __declspec(dllimport)
#else
    #define HERMES_DLL_EXPORT __attribute__((visibility("default")))
    #define HERMES_DLL_IMPORT extern
#endif

#if defined(HERMES_BUILD_DLL)
    #define HERMES_API HERMES_DLL_EXPORT
#elif defined(HERMES_DLL)
    #define HERMES_API HERMES_DLL_IMPORT
#elif defined(HERMES_STATIC)
    #define HERMES_API static
#else
    #define HERMES_API extern
#endif


/*---------------------------------------------------------------------------*/

#if !HERMES_OS_WINDOWS
    #error "HERMES: Currently only supports Windows"
#endif

/*---------------------------------------------------------------------------*/

#ifndef AETHER_H_
    #include "aether/aether.h"
#endif

#ifndef internal
    #define internal static
#endif // internal

#ifndef global
    #define global   static
#endif // global

#ifndef persist
    #define persist  static
#endif // persist

#if HERMES_LANG_CPP
extern "C"
{
#endif // HERMES_LANG_CPP


/*-------- S E R I A L -------------------------------------------------------*/

typedef u8 SerialParity;
enum SerialParity_
{
    SerialParity_None = 0,
    SerialParity_Even,
    SerialParity_Odd,
};

typedef u8 SerialMode;
enum SerialMode_
{
    SerialMode_RS232 = 0, /* full duplex, no direction control        */
    SerialMode_RS485,     /* half duplex; serial_write drives RTS for TX/RX turnaround */
};

typedef u8 SerialTimeoutMode;
enum SerialTimeoutMode_
{
    SerialTimeoutMode_Block = 0, /* wait indefinitely; timeout_ms ignored (default) */
    SerialTimeoutMode_Timeout,   /* wait up to timeout_ms; SerialResult_Timeout if nothing arrives */
    SerialTimeoutMode_Poll,      /* return immediately with whatever's buffered; timeout_ms ignored */
};

typedef u8 SerialStopBits;
enum SerialStopBits_
{
    SerialStopBits_One = 0, /* default on zero-fill */
    SerialStopBits_OneFive,
    SerialStopBits_Two,
};

typedef struct SerialPort { u64 handle; } SerialPort; /* {0} = invalid */

typedef struct SerialConfig
{
    u32               baud;
    SerialParity      parity;
    SerialStopBits    stop_bits;
    SerialMode        serial_mode;
    SerialTimeoutMode timeout_mode; /* Block (default) / Timeout / Poll */
    u32               timeout_ms;   /* only meaningful when timeout_mode == Timeout */
} SerialConfig;

HERMES_API SerialPort serial_open(str8 device, SerialConfig cfg); /* {0} on failure; device e.g. STR("COM10") */
HERMES_API void       serial_close(SerialPort* p);                /* zeros the handle, cross-thread cancellation */
HERMES_API b8         serial_valid(SerialPort p);

typedef u8 SerialResult;
enum SerialResult_
{
    SerialResult_OK = 0,
    SerialResult_Timeout, /* read/write timed out, no data moved -- not an error */
    SerialResult_Error,   /* port is dead; close it */
};

/* Blocking, with a timeout. Partial reads/writes are normal for serial --
 * always check out_recv/out_sent, do not assume the whole buffer moved. */
HERMES_API SerialResult serial_read(SerialPort p, void* buf, u64 cap, u64* out_recv);
HERMES_API SerialResult serial_write(SerialPort p, const void* src, u64 len, u64* out_sent); /* RS485 turnaround handled internally per the port's configured mode */

#if HERMES_LANG_CPP
}
#endif // HERMES_LANG_CPP

#endif // HERMES_H_

/*---------------------------------------------------------------------------*/

#if defined(HERMES_IMPLEMENTATION) && !defined(HERMES_IMPLEMENTATION_DONE)
#define HERMES_IMPLEMENTATION_DONE

/*---------------------------------------------------------------------------*/
/* --- P L A T F O R M ----------------------------------------------------- */
/*---------------------------------------------------------------------------*/
#if HERMES_OS_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif // WIN32_LEAN_AND_MEAN

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif // NOMINMAX
    #include <windows.h>

    internal u64 os_serial_handle_from_raw_(HANDLE h) { return (h == INVALID_HANDLE_VALUE) ? 0 : (u64)(uintptr_t)h; }
#endif //HERMES_OS_WINDOWS


#if HERMES_LANG_CPP
extern "C"
{
#endif // HERMES_LANG_CPP

internal void os_serial_port_close(u64 h)
{
#if HERMES_OS_WINDOWS
    CloseHandle((HANDLE)(uintptr_t)h);
#else
    #error "HERMES: serial port close not implemented on this platform"
#endif
}

internal u64 os_serial_port_open(str8 device, SerialConfig cfg)
{
#if HERMES_OS_WINDOWS

    char path[32];
    if (device.size > sizeof(path) - 5) return 0; /* "\\.\" (4) + device + NUL (1) must fit */

    memcpy(path, "\\\\.\\", 4);
    memcpy(path+4, device.data, device.size);
    path[4+device.size] = '\0';

    HANDLE h = CreateFileA(path, GENERIC_READ|GENERIC_WRITE, 0 /*no sharing*/, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;

    DCB dcb = {0};
    if (!GetCommState(h, &dcb))
    {
        os_serial_port_close(os_serial_handle_from_raw_(h));
        return 0;
    }

    dcb.BaudRate        = (DWORD)cfg.baud;
    dcb.ByteSize        = 8;
    dcb.fBinary         = TRUE;
    dcb.fOutxCtsFlow    = FALSE;
    dcb.fOutxDsrFlow    = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX           = FALSE;
    dcb.fInX            = FALSE;
    dcb.fDtrControl     = DTR_CONTROL_ENABLE;   /* todo(chris): non-configurable for now */
    dcb.fRtsControl     = (cfg.serial_mode == SerialMode_RS485) ? RTS_CONTROL_TOGGLE : RTS_CONTROL_ENABLE;
    dcb.fParity         = (cfg.parity != SerialParity_None);

    switch (cfg.parity)
    {
        case SerialParity_None: dcb.Parity = NOPARITY; break;
        case SerialParity_Even: dcb.Parity = EVENPARITY; break;
        case SerialParity_Odd:  dcb.Parity = ODDPARITY; break;
    }
    switch (cfg.stop_bits)
    {
        case SerialStopBits_One:     dcb.StopBits = ONESTOPBIT; break;
        case SerialStopBits_OneFive: dcb.StopBits = ONE5STOPBITS; break;
        case SerialStopBits_Two:     dcb.StopBits = TWOSTOPBITS; break;
    }

    if (!SetCommState(h, &dcb))
    {
        os_serial_port_close(os_serial_handle_from_raw_(h));
        return 0;
    }

    COMMTIMEOUTS to = {0};
    switch(cfg.timeout_mode)
    {
        case SerialTimeoutMode_Block:    break;
        case SerialTimeoutMode_Timeout:  to.ReadTotalTimeoutConstant = cfg.timeout_ms; break;
        case SerialTimeoutMode_Poll:     to.ReadIntervalTimeout = MAXDWORD; break;
    }

    if (!SetCommTimeouts((HANDLE)(uintptr_t)h, &to))
    {
        os_serial_port_close(os_serial_handle_from_raw_(h));
        return 0;
    }

    return os_serial_handle_from_raw_(h);
#else
    #error "HERMES: serial port open not implemented on this platform"
#endif
}


internal SerialResult os_serial_read(u64 h, u8* buf, u64 cap, u64* out_recv)
{
#if HERMES_OS_WINDOWS
    DWORD chunk = (cap > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (DWORD)cap; /* ReadFile wants DWORD, cap is u64 */
    DWORD read  = 0;
    BOOL  ok    = ReadFile((HANDLE)(uintptr_t)h, buf, chunk, &read, NULL);

    if (!ok)      return SerialResult_Error;
    if (out_recv) *out_recv = (u64)read;

    return (read == 0) ? SerialResult_Timeout : SerialResult_OK;

#else
    #error "HERMES: serial read not implemented on this platform"
#endif
}

internal SerialResult os_serial_write(u64 h, const void* src, u64 len, u64* out_sent)
{
#if HERMES_OS_WINDOWS
    DWORD chunk   = (len > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (DWORD)len; /* WriteFile wants DWORD, len is u64 */
    DWORD written = 0;
    BOOL  ok      = WriteFile((HANDLE)(uintptr_t)h, src, chunk, &written, NULL);

    if (!ok)      return SerialResult_Error;
    if (out_sent) *out_sent = (u64)written;

    return SerialResult_OK;
#else
    #error "HERMES: serial write not implemented on this platform"
#endif
}

/* ------------------------------------------------------------------------- */
/* --- S E R I A L --------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

HERMES_API SerialPort serial_open(str8 device, SerialConfig cfg)
{

    SerialPort p;
    p.handle = os_serial_port_open(device, cfg);
    return p;
}

HERMES_API void serial_close(SerialPort* p)
{
    if (!p) return;

    u64 h = atomic_load_acq_u64(&p->handle);
    if (!h) return;

    if (!atomic_cas_u64(&p->handle, h, 0)) return; /* lost the race, someone else is closing it */
    os_serial_port_close(h);
}

HERMES_API b8 serial_valid(SerialPort p)
{
    return p.handle != 0;
}

HERMES_API SerialResult serial_read(SerialPort p, void* buf, u64 cap, u64* out_recv)
{
    return os_serial_read(p.handle, (u8*)buf, cap, out_recv);
}

HERMES_API SerialResult serial_write(SerialPort p, const void* src, u64 len, u64* out_sent)
{
    return os_serial_write(p.handle, src, len, out_sent);

}

#if HERMES_LANG_CPP
}
#endif // HERMES_LANG_CPP

#endif //HERMES_IMPLEMENTATION
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
