#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "amitcp13/bsdsocket.h"
#include "net.h"

#ifndef SOL_SOCKET
#define SOL_SOCKET AMITCP13_SOL_SOCKET
#endif
#ifndef SO_ERROR
#define SO_ERROR AMITCP13_SO_ERROR
#endif
#ifndef FIONBIO
#define FIONBIO AMITCP13_FIONBIO
#endif

#ifndef DCTELNET13_NET_DEBUG
#define DCTELNET13_NET_DEBUG 0
#endif

#if DCTELNET13_NET_DEBUG
static void net_debug_s(const char *s)
{
    UWORD len;

    if (!s)
        return;
    len = 0;
    while (s[len])
        ++len;
    if (len)
        Write(Output(), (APTR)s, len);
}

static void net_debug_i(const char *prefix, int value)
{
    char buf[16];
    int i;
    int neg;
    ULONG v;

    net_debug_s(prefix);
    neg = value < 0;
    v = neg ? (ULONG)(-value) : (ULONG)value;
    i = sizeof(buf) - 1;
    buf[i] = 0;
    do {
        --i;
        buf[i] = (char)('0' + (v % 10UL));
        v /= 10UL;
    } while (v && i > 0);
    if (neg && i > 0)
        buf[--i] = '-';
    net_debug_s(&buf[i]);
    net_debug_s("\n");
}
#else
#define net_debug_s(s) ((void)0)
#define net_debug_i(p,v) ((void)0)
#endif

static struct Amitcp13BsdSockAddrIn g_addr;
static struct Amitcp13BsdFdSet g_rfds;
static struct Amitcp13BsdFdSet g_wfds;
static struct Amitcp13BsdTimeVal g_timeout;
static ULONG g_wait_signals;
static LONG g_one;
static int g_so_error;
static int g_so_error_len;

static int call_socket(struct Library *base, int domain, int type, int protocol)
{
    register int d0 __asm("d0") = domain;
    register int d1 __asm("d1") = type;
    register int d2 __asm("d2") = protocol;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-30:W)"
        : "+r" (d0), "+r" (d1), "+r" (d2)
        : "r" (a6)
        : "a0", "a1", "cc", "memory");
    return d0;
}

static int call_connect(struct Library *base, int fd,
                        const struct Amitcp13BsdSockAddr *addr, int addrlen)
{
    register int d0 __asm("d0") = fd;
    register const struct Amitcp13BsdSockAddr *a0 __asm("a0") = addr;
    register int d1 __asm("d1") = addrlen;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-54:W)"
        : "+r" (d0), "+r" (a0), "+r" (d1)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_send(struct Library *base, int fd, const void *buf, int len, int flags)
{
    register int d0 __asm("d0") = fd;
    register const void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-66:W)"
        : "+r" (d0), "+r" (a0), "+r" (d1), "+r" (d2)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_recv(struct Library *base, int fd, void *buf, int len, int flags)
{
    register int d0 __asm("d0") = fd;
    register void *a0 __asm("a0") = buf;
    register int d1 __asm("d1") = len;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-78:W)"
        : "+r" (d0), "+r" (a0), "+r" (d1), "+r" (d2)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_ioctl(struct Library *base, int fd, ULONG request, void *argp)
{
    register int d0 __asm("d0") = fd;
    register ULONG d1 __asm("d1") = request;
    register void *a0 __asm("a0") = argp;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-114:W)"
        : "+r" (d0), "+r" (d1), "+r" (a0)
        : "r" (a6)
        : "a1", "cc", "memory");
    return d0;
}

static int call_close_socket(struct Library *base, int fd)
{
    register int d0 __asm("d0") = fd;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-120:W)"
        : "+r" (d0)
        : "r" (a6)
        : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static int call_waitselect(struct Library *base, int nfds,
                           struct Amitcp13BsdFdSet *readfds,
                           struct Amitcp13BsdFdSet *writefds,
                           const struct Amitcp13BsdTimeVal *timeout)
{
    register int d0 __asm("d0") = nfds;
    register ULONG *d1 __asm("d1") = &g_wait_signals;
    register struct Amitcp13BsdFdSet *a0 __asm("a0") = readfds;
    register struct Amitcp13BsdFdSet *a1 __asm("a1") = writefds;
    register struct Amitcp13BsdFdSet *a2 __asm("a2") = 0;
    register const struct Amitcp13BsdTimeVal *a3 __asm("a3") = timeout;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-126:W)"
        : "+r" (d0), "+r" (d1), "+r" (a0), "+r" (a1), "+r" (a2), "+r" (a3)
        : "r" (a6)
        : "cc", "memory");
    return d0;
}

static int call_errno(struct Library *base)
{
    register int d0 __asm("d0");
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-162:W)"
        : "=r" (d0)
        : "r" (a6)
        : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

static struct hostent *call_gethostbyname(struct Library *base, const char *name)
{
    register const char *a0 __asm("a0") = name;
    register struct Library *a6 __asm("a6") = base;
    register struct hostent *d0 __asm("d0");

    __asm volatile ("jsr a6@(-210:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

#if DCTELNET13_NET_DEBUG
static void net_debug_ret_errno(struct Library *base, const char *name, int ret)
{
    int err;

    net_debug_i(name, ret);
    if (ret < 0) {
        err = call_errno(base);
        net_debug_i("DCTELNET13 NET errno=", err);
    }
}
#else
#define net_debug_ret_errno(base,name,ret) ((void)0)
#endif

static int call_getsockopt(struct Library *base, int fd, int level, int optname,
                           void *optval, int *optlen)
{
    register int d0 __asm("d0") = fd;
    register int d1 __asm("d1") = level;
    register int d2 __asm("d2") = optname;
    register void *a0 __asm("a0") = optval;
    register int *a1 __asm("a1") = optlen;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-96:W)"
        : "+r" (d0), "+r" (d1), "+r" (d2), "+r" (a0), "+r" (a1)
        : "r" (a6)
        : "cc", "memory");
    return d0;
}

void dct13_net_init(struct Dct13Net *net)
{
    net->base = 0;
    net->fd = -1;
    net->last_errno = 0;
    net->last_ip = 0;
}

int dct13_net_open(struct Dct13Net *net)
{
    if (net->base)
        return DCT13_NET_OK;
    net->base = OpenLibrary((STRPTR)"bsdsocket.library", 0);
    if (!net->base) {
        net->last_errno = -1;
        net_debug_s("MiniTelnet bsdsocket open failed\n");
        return DCT13_NET_ERROR;
    }
    net_debug_s("MiniTelnet bsdsocket open ok\n");
    return DCT13_NET_OK;
}

void dct13_net_close(struct Dct13Net *net)
{
    if (net->base && net->fd >= 0)
        call_close_socket(net->base, net->fd);
    net->fd = -1;
}

void dct13_net_cleanup(struct Dct13Net *net)
{
    dct13_net_close(net);
    if (net->base) {
        CloseLibrary(net->base);
        net->base = 0;
    }
}

int dct13_net_connected(const struct Dct13Net *net)
{
    return net->fd >= 0;
}

static int wait_ready(struct Dct13Net *net, int fd, int write, LONG sec)
{
    AMITCP13_BSD_FD_ZERO(&g_rfds);
    AMITCP13_BSD_FD_ZERO(&g_wfds);
    if (write)
        AMITCP13_BSD_FD_SET(fd, &g_wfds);
    else
        AMITCP13_BSD_FD_SET(fd, &g_rfds);
    g_timeout.tv_sec = sec;
    g_timeout.tv_usec = 0;
    g_wait_signals = 0;
    return call_waitselect(net->base, fd + 1,
        write ? 0 : &g_rfds,
        write ? &g_wfds : 0,
        &g_timeout);
}

int dct13_net_connect(struct Dct13Net *net, const char *host, UWORD port, LONG timeout_sec)
{
    struct hostent *he;
    int fd;
    int r;
    int err;

    if (dct13_net_open(net) != DCT13_NET_OK)
        return DCT13_NET_ERROR;
    dct13_net_close(net);

    net_debug_s("MiniTelnet DNS start\n");
    he = call_gethostbyname(net->base, host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        net->last_errno = call_errno(net->base);
        net_debug_i("MiniTelnet DNS failed errno=", net->last_errno);
        return DCT13_NET_ERROR;
    }
    net->last_ip = *(ULONG *)he->h_addr_list[0];
    net_debug_s("MiniTelnet DNS ok\n");

    fd = call_socket(net->base, AMITCP13_AF_INET, AMITCP13_SOCK_STREAM, AMITCP13_IPPROTO_TCP);
    if (fd < 0) {
        net->last_errno = call_errno(net->base);
        net_debug_i("MiniTelnet socket failed errno=", net->last_errno);
        return DCT13_NET_ERROR;
    }
    net_debug_i("MiniTelnet socket fd=", fd);

    g_one = 1;
    r = call_ioctl(net->base, fd, FIONBIO, &g_one);
    net_debug_i("DCTELNET13 NET IoctlSocket FIONBIO fd=", fd);
    net_debug_i("DCTELNET13 NET IoctlSocket request=", FIONBIO);
    net_debug_ret_errno(net->base, "DCTELNET13 NET IoctlSocket ret=", r);

    g_addr.sin_len = sizeof(g_addr);
    g_addr.sin_family = AMITCP13_AF_INET;
    g_addr.sin_port = port;
    g_addr.sin_addr.s_addr = net->last_ip;

    r = call_connect(net->base, fd, (const struct Amitcp13BsdSockAddr *)&g_addr, sizeof(g_addr));
    net_debug_ret_errno(net->base, "DCTELNET13 NET connect ret=", r);
    if (r < 0) {
        err = call_errno(net->base);
        net->last_errno = err;
        net_debug_i("DCTELNET13 NET connect errno=", err);
        if (err != AMITCP13_EINPROGRESS && err != AMITCP13_EALREADY) {
            call_close_socket(net->base, fd);
            return DCT13_NET_ERROR;
        }
        r = wait_ready(net, fd, 1, timeout_sec);
        net_debug_i("DCTELNET13 NET WaitSelect nfds=", fd + 1);
        net_debug_s("DCTELNET13 NET WaitSelect mode=write\n");
        net_debug_ret_errno(net->base, "DCTELNET13 NET WaitSelect ret=", r);
        if (r <= 0) {
            net->last_errno = call_errno(net->base);
            call_close_socket(net->base, fd);
            return DCT13_NET_ERROR;
        }
        if (!AMITCP13_BSD_FD_ISSET(fd, &g_wfds)) {
            net->last_errno = 0;
            call_close_socket(net->base, fd);
            return DCT13_NET_ERROR;
        }
        g_so_error = -1;
        g_so_error_len = sizeof(g_so_error);
        net_debug_i("DCTELNET13 NET getsockopt fd=", fd);
        net_debug_i("DCTELNET13 NET getsockopt level=", AMITCP13_SOL_SOCKET);
        net_debug_i("DCTELNET13 NET getsockopt opt=", AMITCP13_SO_ERROR);
        r = call_getsockopt(net->base, fd, AMITCP13_SOL_SOCKET, AMITCP13_SO_ERROR,
            &g_so_error, &g_so_error_len);
        net_debug_ret_errno(net->base, "DCTELNET13 NET getsockopt ret=", r);
        net_debug_i("DCTELNET13 NET SO_ERROR value=", g_so_error);
        if (r < 0 || g_so_error != 0) {
            net->last_errno = (r < 0) ? call_errno(net->base) : g_so_error;
            call_close_socket(net->base, fd);
            return DCT13_NET_ERROR;
        }
    }

    net->fd = fd;
    net->last_errno = 0;
    net_debug_s("MiniTelnet connected\n");
    return DCT13_NET_OK;
}

int dct13_net_send(struct Dct13Net *net, const UBYTE *buf, UWORD len)
{
    int r;

    if (!net->base || net->fd < 0)
        return DCT13_NET_CLOSED;
    net_debug_i("DCTELNET13 NET send fd=", net->fd);
    net_debug_i("DCTELNET13 NET send len=", len);
    net_debug_i("DCTELNET13 NET send flags=", 0);
    r = call_send(net->base, net->fd, buf, len, 0);
    net_debug_ret_errno(net->base, "DCTELNET13 NET send ret=", r);
    if (r >= 0)
        return r;
    net->last_errno = call_errno(net->base);
    if (net->last_errno == AMITCP13_EWOULDBLOCK || net->last_errno == AMITCP13_EAGAIN)
        return DCT13_NET_WOULDBLOCK;
    net_debug_i("DCTELNET13 NET send errno=", net->last_errno);
    dct13_net_close(net);
    return DCT13_NET_ERROR;
}

int dct13_net_recv(struct Dct13Net *net, UBYTE *buf, UWORD len)
{
    int r;

    if (!net->base || net->fd < 0)
        return DCT13_NET_CLOSED;
    r = wait_ready(net, net->fd, 0, 0);
    net_debug_i("DCTELNET13 NET WaitSelect nfds=", net->fd + 1);
    net_debug_s("DCTELNET13 NET WaitSelect mode=read\n");
    net_debug_ret_errno(net->base, "DCTELNET13 NET WaitSelect ret=", r);
    if (r <= 0) {
        if (r < 0) {
            net->last_errno = call_errno(net->base);
            if (net->last_errno != 0 &&
                net->last_errno != AMITCP13_EWOULDBLOCK &&
                net->last_errno != AMITCP13_EAGAIN &&
                net->last_errno != AMITCP13_EINPROGRESS) {
                net_debug_i("DCTELNET13 NET WaitSelect errno=", net->last_errno);
                dct13_net_close(net);
                return DCT13_NET_ERROR;
            }
        }
        return DCT13_NET_WOULDBLOCK;
    }
    if (!AMITCP13_BSD_FD_ISSET(net->fd, &g_rfds)) {
        net_debug_s("DCTELNET13 NET WaitSelect no readable fd\n");
        return DCT13_NET_WOULDBLOCK;
    }

    net_debug_i("DCTELNET13 NET recv fd=", net->fd);
    net_debug_i("DCTELNET13 NET recv len=", len);
    net_debug_i("DCTELNET13 NET recv flags=", 0);
    r = call_recv(net->base, net->fd, buf, len, 0);
    net_debug_ret_errno(net->base, "DCTELNET13 NET recv ret=", r);
    if (r > 0)
        return r;
    if (r == 0) {
        dct13_net_close(net);
        return DCT13_NET_CLOSED;
    }
    net->last_errno = call_errno(net->base);
    if (net->last_errno == 0 ||
        net->last_errno == AMITCP13_EWOULDBLOCK ||
        net->last_errno == AMITCP13_EAGAIN ||
        net->last_errno == AMITCP13_EINPROGRESS) {
        net_debug_i("DCTELNET13 NET recv no-data errno=", net->last_errno);
        return DCT13_NET_WOULDBLOCK;
    }
    net_debug_i("DCTELNET13 NET recv errno=", net->last_errno);
    dct13_net_close(net);
    return DCT13_NET_ERROR;
}
