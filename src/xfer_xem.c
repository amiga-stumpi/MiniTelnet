#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <proto/exec.h>

#include "xfer_xem.h"

#define XEM_LIBRARY_VERSION 0

struct xem_option;
struct XEmulatorMacroKey;

struct XEM_IO {
    struct Window *xem_window;
    struct TextFont *xem_font;
    APTR xem_console;
    ULONG xem_obsolete1;
    ULONG *xem_signal;
    UWORD xem_screendepth;
    UWORD xem_pad;
    LONG (*xem_sread)(UBYTE *buffer, LONG size, LONG timeout);
    LONG (*xem_swrite)(UBYTE *buffer, LONG size);
    LONG (*xem_sflush)(void);
    LONG (*xem_sbreak)(void);
    LONG (*xem_squery)(void);
    void (*xem_sstart)(void);
    LONG (*xem_sstop)(void);
    void (*xem_tbeep)(ULONG ntimes, ULONG delay);
    LONG (*xem_tgets)(UBYTE *prompt, UBYTE *buffer, ULONG buflen);
    ULONG (*xem_toptions)(LONG n, struct xem_option *opt[]);
    LONG (*xem_process_macrokeys)(struct XEmulatorMacroKey *key);
};

static struct Library *g_xem_base;
static struct XEM_IO g_xem_io;
static ULONG g_xem_signal;
static struct Dct13Net *g_xem_net;

static LONG xem_sread_cb(register UBYTE *buffer __asm("a0"),
                         register LONG size __asm("d0"),
                         register LONG timeout __asm("d1"))
{
    (void)buffer;
    (void)size;
    (void)timeout;
    return 0;
}

static LONG xem_swrite_cb(register UBYTE *buffer __asm("a0"),
                          register LONG size __asm("d0"))
{
    LONG sent;
    int rc;

    if (!buffer || size <= 0 || !g_xem_net || !dct13_net_connected(g_xem_net))
        return -1;
    sent = 0;
    while (sent < size) {
        rc = dct13_net_send(g_xem_net, &buffer[sent], (UWORD)(size - sent > 512 ? 512 : size - sent));
        if (rc < 0)
            return -1;
        sent += rc;
    }
    return 0;
}

static LONG xem_sflush_cb(void)
{
    return 0;
}

static LONG xem_sbreak_cb(void)
{
    return 0;
}

static LONG xem_squery_cb(void)
{
    return 0;
}

static void xem_sstart_cb(void)
{
}

static LONG xem_sstop_cb(void)
{
    return 0;
}

static void xem_tbeep_cb(register ULONG ntimes __asm("d0"),
                         register ULONG delay __asm("d1"))
{
    (void)ntimes;
    (void)delay;
}

static LONG xem_tgets_cb(register UBYTE *prompt __asm("a0"),
                         register UBYTE *buffer __asm("a1"),
                         register ULONG buflen __asm("d0"))
{
    (void)prompt;
    if (buffer && buflen)
        buffer[0] = 0;
    return 0;
}

static ULONG xem_toptions_cb(register LONG n __asm("d0"),
                             register struct xem_option **opt __asm("a0"))
{
    (void)n;
    (void)opt;
    return 0;
}

static LONG xem_macro_cb(register struct XEmulatorMacroKey *key __asm("a0"))
{
    (void)key;
    return 0;
}

static void clear_xem_io(void)
{
    UBYTE *p;
    ULONG i;

    p = (UBYTE *)&g_xem_io;
    for (i = 0; i < sizeof(g_xem_io); ++i)
        p[i] = 0;
}

static int check_xem_dependency(const char *name)
{
    struct Library *base;

    base = OpenLibrary((STRPTR)name, 33);
    if (!base)
        return 0;
    CloseLibrary(base);
    return 1;
}

static UWORD window_screen_depth(struct Window *win)
{
    UWORD depth;

    depth = 1;
    if (win && win->WScreen)
        depth = win->WScreen->BitMap.Depth;
    else if (win && win->RPort && win->RPort->BitMap)
        depth = win->RPort->BitMap->Depth;
    if (depth < 1)
        depth = 1;
    if (depth > 4)
        depth = 4;
    return depth;
}

static int call_xem_setup(struct Library *base, struct XEM_IO *io)
{
    register struct XEM_IO *a0 __asm("a0") = io;
    register struct Library *a6 __asm("a6") = base;
    register int d0 __asm("d0");

    __asm volatile ("jsr a6@(-30:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

static int call_xem_open_console(struct Library *base, struct XEM_IO *io)
{
    register struct XEM_IO *a0 __asm("a0") = io;
    register struct Library *a6 __asm("a6") = base;
    register int d0 __asm("d0");

    __asm volatile ("jsr a6@(-36:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

static void call_xem_close_console(struct Library *base, struct XEM_IO *io)
{
    register struct XEM_IO *a0 __asm("a0") = io;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-42:W)"
        : "+r" (a0)
        : "r" (a6)
        : "d0", "d1", "a1", "cc", "memory");
}

static void call_xem_cleanup(struct Library *base, struct XEM_IO *io)
{
    register struct XEM_IO *a0 __asm("a0") = io;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-48:W)"
        : "+r" (a0)
        : "r" (a6)
        : "d0", "d1", "a1", "cc", "memory");
}

static void call_xem_write(struct Library *base, struct XEM_IO *io, UBYTE *data, LONG len)
{
    register struct XEM_IO *a0 __asm("a0") = io;
    register UBYTE *a1 __asm("a1") = data;
    register LONG d0 __asm("d0") = len;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-54:W)"
        : "+r" (a0), "+r" (a1), "+r" (d0)
        : "r" (a6)
        : "d1", "cc", "memory");
}

static void call_xem_clear(struct Library *base, struct XEM_IO *io)
{
    register struct XEM_IO *a0 __asm("a0") = io;
    register struct Library *a6 __asm("a6") = base;

    __asm volatile ("jsr a6@(-84:W)"
        : "+r" (a0)
        : "r" (a6)
        : "d0", "d1", "a1", "cc", "memory");
}

int dct13_xem_open(struct Window *win, struct TextFont *font, struct Dct13Net *net,
                   const char *library_name)
{
    if (!win || !library_name || !library_name[0])
        return DCT13_XEM_ERROR;
    dct13_xem_close();
    if (!check_xem_dependency("keymap.library"))
        return DCT13_XEM_NO_KEYMAP;
    if (!check_xem_dependency("diskfont.library"))
        return DCT13_XEM_NO_DISKFONT;
    g_xem_base = OpenLibrary((STRPTR)library_name, XEM_LIBRARY_VERSION);
    if (!g_xem_base)
        return DCT13_XEM_NO_LIBRARY;
    clear_xem_io();
    g_xem_net = net;
    g_xem_signal = 0;
    g_xem_io.xem_window = win;
    g_xem_io.xem_font = font;
    g_xem_io.xem_signal = &g_xem_signal;
    g_xem_io.xem_screendepth = window_screen_depth(win);
    g_xem_io.xem_sread = xem_sread_cb;
    g_xem_io.xem_swrite = xem_swrite_cb;
    g_xem_io.xem_sflush = xem_sflush_cb;
    g_xem_io.xem_sbreak = xem_sbreak_cb;
    g_xem_io.xem_squery = xem_squery_cb;
    g_xem_io.xem_sstart = xem_sstart_cb;
    g_xem_io.xem_sstop = xem_sstop_cb;
    g_xem_io.xem_tbeep = xem_tbeep_cb;
    g_xem_io.xem_tgets = xem_tgets_cb;
    g_xem_io.xem_toptions = xem_toptions_cb;
    g_xem_io.xem_process_macrokeys = xem_macro_cb;
    if (!call_xem_setup(g_xem_base, &g_xem_io)) {
        dct13_xem_close();
        return DCT13_XEM_SETUP_FAILED;
    }
    if (!call_xem_open_console(g_xem_base, &g_xem_io)) {
        dct13_xem_close();
        return DCT13_XEM_OPEN_CONSOLE_FAILED;
    }
    return DCT13_XEM_OK;
}

void dct13_xem_close(void)
{
    if (g_xem_base) {
        call_xem_close_console(g_xem_base, &g_xem_io);
        call_xem_cleanup(g_xem_base, &g_xem_io);
        CloseLibrary(g_xem_base);
        g_xem_base = 0;
    }
    g_xem_net = 0;
    clear_xem_io();
}

int dct13_xem_active(void)
{
    return g_xem_base != 0;
}

void dct13_xem_write(const UBYTE *data, UWORD len)
{
    if (!g_xem_base || !data || len == 0)
        return;
    call_xem_write(g_xem_base, &g_xem_io, (UBYTE *)data, len);
}

void dct13_xem_clear(void)
{
    if (g_xem_base)
        call_xem_clear(g_xem_base, &g_xem_io);
}
