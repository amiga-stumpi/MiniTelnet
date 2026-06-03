#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "amitcp13/bsdsocket.h"
#include "xfer_xpr.h"

#define XPRU_PROTOCOL 0x00000001L
#define XPRU_FILENAME 0x00000002L
#define XPRU_FILESIZE 0x00000004L
#define XPRU_MSG 0x00000008L
#define XPRU_ERRORMSG 0x00000010L
#define XPRU_BYTES 0x00000080L
#define XPR_EXTENSION 4L

struct XPR_UPDATE {
    long xpru_updatemask;
    char *xpru_protocol;
    char *xpru_filename;
    long xpru_filesize;
    char *xpru_msg;
    char *xpru_errormsg;
    long xpru_blocks;
    long xpru_blocksize;
    long xpru_bytes;
    long xpru_errors;
    long xpru_timeouts;
    long xpru_packettype;
    long xpru_packetdelay;
    long xpru_chardelay;
    char *xpru_blockcheck;
    char *xpru_expecttime;
    char *xpru_elapsedtime;
    long xpru_datarate;
    long xpru_reserved1;
    long xpru_reserved2;
    long xpru_reserved3;
    long xpru_reserved4;
    long xpru_reserved5;
};

struct XPR_IO {
    char *xpr_filename;
    long (*xpr_fopen)(char *filename, char *accessmode);
    long (*xpr_fclose)(long filepointer);
    long (*xpr_fread)(char *buffer, long size, long count, long fileptr);
    long (*xpr_fwrite)(char *buffer, long size, long count, long fileptr);
    long (*xpr_sread)(char *buffer, long size, long timeout);
    long (*xpr_swrite)(char *buffer, long size);
    long (*xpr_sflush)(void);
    long (*xpr_update)(struct XPR_UPDATE *updatestruct);
    long (*xpr_chkabort)(void);
    void (*xpr_chkmisc)(void);
    long (*xpr_gets)(char *prompt, char *buffer);
    long (*xpr_setserial)(long newstatus);
    long (*xpr_ffirst)(char *buffer, char *pattern);
    long (*xpr_fnext)(long oldstate, char *buffer, char *pattern);
    long (*xpr_finfo)(char *filename, long typeofinfo);
    long (*xpr_fseek)(long fileptr, long offset, long origin);
    long xpr_extension;
    long *xpr_data;
    long (*xpr_options)(long n, void *opt);
    long (*xpr_unlink)(char *filename);
    long (*xpr_squery)(void);
    long (*xpr_getptr)(long type);
};

static struct Dct13Net *g_xpr_net;
static Dct13XferStatusFunc g_xpr_status_func;
static void *g_xpr_status_user;
static struct Library *g_xpr_base;
static struct XPR_IO g_xio;
static UBYTE g_xpr_ff_pending;
static char g_status[96];
static char g_byte_buf[512];
static char g_send_buf[1024];
static UBYTE g_prefix_buf[256];
static UWORD g_prefix_len;
static UWORD g_prefix_pos;
static struct Amitcp13BsdFdSet g_rfds;
static struct Amitcp13BsdFdSet g_wfds;
static struct Amitcp13BsdTimeVal g_timeout;
static ULONG g_wait_signals;

static void status_text(const char *text)
{
    if (g_xpr_status_func)
        g_xpr_status_func(text, g_xpr_status_user);
}

static void append_text(char *dst, ULONG dst_size, UWORD *pos, const char *src)
{
    UWORD i;

    i = 0;
    while (src && src[i] && ((ULONG)(*pos) + 1UL) < dst_size)
        dst[(*pos)++] = src[i++];
    dst[*pos] = 0;
}

static void append_dec(char *dst, ULONG dst_size, UWORD *pos, ULONG value)
{
    char digits[12];
    UWORD i;

    i = sizeof(digits) - 1;
    digits[i] = 0;
    do {
        --i;
        digits[i] = (char)('0' + (value % 10UL));
        value /= 10UL;
    } while (value && i > 0);
    append_text(dst, dst_size, pos, &digits[i]);
}

static void status_xfer_bytes(long bytes)
{
    UWORD pos;

    pos = 0;
    append_text(g_status, sizeof(g_status), &pos, "ZModem receiving ");
    if (bytes >= 0)
        append_dec(g_status, sizeof(g_status), &pos, (ULONG)bytes);
    append_text(g_status, sizeof(g_status), &pos, " bytes");
    status_text(g_status);
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

static int wait_socket(int write, LONG usec)
{
    LONG sec;
    LONG rem;

    if (!g_xpr_net || !g_xpr_net->base || g_xpr_net->fd < 0)
        return -1;
    AMITCP13_BSD_FD_ZERO(&g_rfds);
    AMITCP13_BSD_FD_ZERO(&g_wfds);
    if (write)
        AMITCP13_BSD_FD_SET(g_xpr_net->fd, &g_wfds);
    else
        AMITCP13_BSD_FD_SET(g_xpr_net->fd, &g_rfds);
    if (usec < 0)
        usec = 0;
    sec = usec / 1000000L;
    rem = usec % 1000000L;
    g_timeout.tv_sec = sec;
    g_timeout.tv_usec = rem;
    g_wait_signals = 0;
    return call_waitselect(g_xpr_net->base, g_xpr_net->fd + 1,
        write ? 0 : &g_rfds,
        write ? &g_wfds : 0,
        &g_timeout);
}

static long strip_telnet_ff(char *buffer, long length)
{
    long i;
    long j;

    i = 0;
    j = 0;
    while (i < length) {
        if ((UBYTE)buffer[i] == 255 && !g_xpr_ff_pending) {
            g_xpr_ff_pending = 1;
        } else {
            buffer[j++] = buffer[i];
            g_xpr_ff_pending = 0;
        }
        ++i;
    }
    return j;
}

static long xpr_fopen_cb(register char *filename __asm("a0"),
                         register char *accessmode __asm("a1"))
{
    BPTR fh;

    if (!filename || !accessmode)
        return 0;
    if (*accessmode == 'r')
        return (long)Open((STRPTR)filename, MODE_OLDFILE);
    if (*accessmode == 'w') {
        fh = Open((STRPTR)filename, MODE_NEWFILE);
        return (long)fh;
    }
    if (*accessmode == 'a') {
        fh = Open((STRPTR)filename, MODE_READWRITE);
        if (fh)
            Seek(fh, 0, OFFSET_END);
        return (long)fh;
    }
    return 0;
}

static long xpr_fclose_cb(register long filepointer __asm("a0"))
{
    if (filepointer)
        Close((BPTR)filepointer);
    return 0;
}

static long xpr_fread_cb(register char *buffer __asm("a0"),
                         register long size __asm("d0"),
                         register long count __asm("d1"),
                         register long fileptr __asm("a1"))
{
    if (!buffer || !fileptr || size <= 0 || count <= 0)
        return 0;
    return Read((BPTR)fileptr, buffer, size * count);
}

static long xpr_fwrite_cb(register char *buffer __asm("a0"),
                          register long size __asm("d0"),
                          register long count __asm("d1"),
                          register long fileptr __asm("a1"))
{
    if (!buffer || !fileptr || size <= 0 || count <= 0)
        return 0;
    return Write((BPTR)fileptr, buffer, size * count);
}

static long xpr_sread_cb(register char *buffer __asm("a0"),
                         register long size __asm("d0"),
                         register long timeout __asm("d1"))
{
    int r;
    long usec;
    UWORD n;

    if (!buffer || size <= 0 || !g_xpr_net || !g_xpr_net->base || g_xpr_net->fd < 0)
        return -1;
    if (g_prefix_pos < g_prefix_len) {
        n = (UWORD)(g_prefix_len - g_prefix_pos);
        if ((long)n > size)
            n = (UWORD)size;
        CopyMem(&g_prefix_buf[g_prefix_pos], buffer, n);
        g_prefix_pos = (UWORD)(g_prefix_pos + n);
        return n;
    }
    usec = timeout;
    if (usec == 0)
        usec = 1000;
    r = wait_socket(0, usec);
    if (r < 0)
        return -1;
    if (r == 0 || !AMITCP13_BSD_FD_ISSET(g_xpr_net->fd, &g_rfds))
        return 0;
    if (size > (long)sizeof(g_byte_buf))
        size = sizeof(g_byte_buf);
    r = call_recv(g_xpr_net->base, g_xpr_net->fd, buffer, (int)size, 0);
    if (r <= 0)
        return r == 0 ? -1 : 0;
    return strip_telnet_ff(buffer, r);
}

static long xpr_swrite_cb(register char *buffer __asm("a0"),
                          register long size __asm("d0"))
{
    long i;
    long j;
    long sent;
    int r;

    if (!buffer || size < 0 || !g_xpr_net || !g_xpr_net->base || g_xpr_net->fd < 0)
        return -1;
    i = 0;
    while (i < size) {
        j = 0;
        while (i < size && j < (long)(sizeof(g_send_buf) - 2)) {
            if ((UBYTE)buffer[i] == 255)
                g_send_buf[j++] = buffer[i];
            g_send_buf[j++] = buffer[i++];
        }
        sent = 0;
        while (sent < j) {
            r = wait_socket(1, 250000L);
            if (r < 0)
                return -1;
            if (r == 0)
                continue;
            r = call_send(g_xpr_net->base, g_xpr_net->fd, &g_send_buf[sent], (int)(j - sent), 0);
            if (r < 0)
                return -1;
            if (r == 0)
                continue;
            sent += r;
        }
    }
    return 0;
}

static long xpr_sflush_cb(void)
{
    int r;

    if (!g_xpr_net || !g_xpr_net->base || g_xpr_net->fd < 0)
        return -1;
    for (;;) {
        r = wait_socket(0, 1000L);
        if (r <= 0 || !AMITCP13_BSD_FD_ISSET(g_xpr_net->fd, &g_rfds))
            break;
        r = call_recv(g_xpr_net->base, g_xpr_net->fd, g_byte_buf, sizeof(g_byte_buf), 0);
        if (r <= 0)
            break;
    }
    return 0;
}

static long xpr_update_cb(register struct XPR_UPDATE *updatestruct __asm("a0"))
{
    UWORD pos;

    if (!updatestruct)
        return 0;
    if ((updatestruct->xpru_updatemask & XPRU_ERRORMSG) && updatestruct->xpru_errormsg) {
        pos = 0;
        append_text(g_status, sizeof(g_status), &pos, "ZModem error: ");
        append_text(g_status, sizeof(g_status), &pos, updatestruct->xpru_errormsg);
        status_text(g_status);
    } else if ((updatestruct->xpru_updatemask & XPRU_MSG) && updatestruct->xpru_msg) {
        pos = 0;
        append_text(g_status, sizeof(g_status), &pos, "ZModem: ");
        append_text(g_status, sizeof(g_status), &pos, updatestruct->xpru_msg);
        status_text(g_status);
    } else if (updatestruct->xpru_updatemask & XPRU_BYTES) {
        status_xfer_bytes(updatestruct->xpru_bytes);
    } else if ((updatestruct->xpru_updatemask & XPRU_FILENAME) && updatestruct->xpru_filename) {
        pos = 0;
        append_text(g_status, sizeof(g_status), &pos, "ZModem file: ");
        append_text(g_status, sizeof(g_status), &pos, updatestruct->xpru_filename);
        status_text(g_status);
    } else if ((updatestruct->xpru_updatemask & XPRU_PROTOCOL) && updatestruct->xpru_protocol) {
        status_text(updatestruct->xpru_protocol);
    }
    return 0;
}

static long xpr_chkabort_cb(void)
{
    return 0;
}

static long xpr_gets_cb(register char *prompt __asm("a0"),
                        register char *buffer __asm("a1"))
{
    (void)prompt;
    if (buffer)
        buffer[0] = 0;
    return 0;
}

static long xpr_setserial_cb(register long newstatus __asm("d0"))
{
    (void)newstatus;
    return 0;
}

static long xpr_finfo_cb(register char *filename __asm("a0"),
                         register long typeofinfo __asm("d0"))
{
    struct FileInfoBlock *fib;
    BPTR lock;
    long result;

    result = 0;
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib)
        return 0;
    lock = Lock((STRPTR)filename, ACCESS_READ);
    if (lock) {
        if (Examine(lock, fib)) {
            if (typeofinfo == 1)
                result = fib->fib_Size;
            else if (typeofinfo == 2)
                result = 1;
        }
        UnLock(lock);
    }
    FreeMem(fib, sizeof(struct FileInfoBlock));
    return result;
}

static long xpr_fseek_cb(register long fileptr __asm("a0"),
                         register long offset __asm("d0"),
                         register long origin __asm("d1"))
{
    long mode;

    if (!fileptr)
        return -1;
    if (origin == 0)
        mode = OFFSET_BEGINNING;
    else if (origin == 1)
        mode = OFFSET_CURRENT;
    else if (origin == 2)
        mode = OFFSET_END;
    else
        return -1;
    return Seek((BPTR)fileptr, offset, mode) == -1 ? -1 : 0;
}

static long xpr_unlink_cb(register char *filename __asm("a0"))
{
    if (!filename)
        return 0;
    return DeleteFile((STRPTR)filename);
}

static long xpr_squery_cb(void)
{
    int r;

    r = wait_socket(0, 1000L);
    if (r < 0)
        return -1;
    if (r > 0 && AMITCP13_BSD_FD_ISSET(g_xpr_net->fd, &g_rfds))
        return 1;
    return 0;
}

static long call_xprotocol_setup(struct Library *base, struct XPR_IO *xio)
{
    register struct XPR_IO *a0 __asm("a0") = xio;
    register struct Library *a6 __asm("a6") = base;
    register long d0 __asm("d0");

    __asm volatile ("jsr a6@(-36:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

static long call_xprotocol_receive(struct Library *base, struct XPR_IO *xio)
{
    register struct XPR_IO *a0 __asm("a0") = xio;
    register struct Library *a6 __asm("a6") = base;
    register long d0 __asm("d0");

    __asm volatile ("jsr a6@(-48:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

static long call_xprotocol_cleanup(struct Library *base, struct XPR_IO *xio)
{
    register struct XPR_IO *a0 __asm("a0") = xio;
    register struct Library *a6 __asm("a6") = base;
    register long d0 __asm("d0");

    __asm volatile ("jsr a6@(-30:W)"
        : "=r" (d0), "+r" (a0)
        : "r" (a6)
        : "d1", "a1", "cc", "memory");
    return d0;
}

int dct13_xpr_available(void)
{
    struct Library *base;

    base = OpenLibrary((STRPTR)"xprzmodem.library", 0);
    if (!base)
        return 0;
    CloseLibrary(base);
    return 1;
}

static void init_xio(void)
{
    UBYTE *p;
    ULONG i;

    p = (UBYTE *)&g_xio;
    for (i = 0; i < sizeof(g_xio); ++i)
        p[i] = 0;
    g_xio.xpr_fopen = xpr_fopen_cb;
    g_xio.xpr_fclose = xpr_fclose_cb;
    g_xio.xpr_fread = xpr_fread_cb;
    g_xio.xpr_fwrite = xpr_fwrite_cb;
    g_xio.xpr_sread = xpr_sread_cb;
    g_xio.xpr_swrite = xpr_swrite_cb;
    g_xio.xpr_sflush = xpr_sflush_cb;
    g_xio.xpr_update = xpr_update_cb;
    g_xio.xpr_chkabort = xpr_chkabort_cb;
    g_xio.xpr_gets = xpr_gets_cb;
    g_xio.xpr_setserial = xpr_setserial_cb;
    g_xio.xpr_finfo = xpr_finfo_cb;
    g_xio.xpr_fseek = xpr_fseek_cb;
    g_xio.xpr_unlink = xpr_unlink_cb;
    g_xio.xpr_squery = xpr_squery_cb;
    g_xio.xpr_extension = XPR_EXTENSION;
}

int dct13_xpr_receive_zmodem_prefixed(struct Dct13Net *net,
                                      Dct13XferStatusFunc status_func,
                                      void *status_user,
                                      const UBYTE *prefix,
                                      UWORD prefix_len)
{
    long rc;
    char setup_opts[] = "TC,OR,B32,FO,AN,DN,KY,SN,RN";

    if (!net || !net->base || net->fd < 0)
        return DCT13_XFER_NOT_CONNECTED;
    if (prefix && prefix_len > 0) {
        if (prefix_len > sizeof(g_prefix_buf))
            prefix_len = sizeof(g_prefix_buf);
        CopyMem((APTR)prefix, g_prefix_buf, prefix_len);
        g_prefix_len = prefix_len;
        g_prefix_pos = 0;
    } else {
        g_prefix_len = 0;
        g_prefix_pos = 0;
    }
    g_xpr_net = net;
    g_xpr_status_func = status_func;
    g_xpr_status_user = status_user;
    g_xpr_ff_pending = 0;
    status_text("Opening xprzmodem.library...");
    g_xpr_base = OpenLibrary((STRPTR)"xprzmodem.library", 0);
    if (!g_xpr_base) {
        status_text("xprzmodem.library missing");
        return DCT13_XFER_NO_LIBRARY;
    }
    init_xio();
    g_xio.xpr_filename = setup_opts;
    rc = call_xprotocol_setup(g_xpr_base, &g_xio);
    if (rc == 0) {
        CloseLibrary(g_xpr_base);
        g_xpr_base = 0;
        status_text("ZModem setup failed");
        return DCT13_XFER_ERROR;
    }
    g_xio.xpr_filename = 0;
    status_text("ZModem receive...");
    rc = call_xprotocol_receive(g_xpr_base, &g_xio);
    call_xprotocol_cleanup(g_xpr_base, &g_xio);
    CloseLibrary(g_xpr_base);
    g_xpr_base = 0;
    if (rc == 0) {
        status_text("ZModem failed");
        return DCT13_XFER_ERROR;
    }
    status_text("ZModem download complete");
    return DCT13_XFER_OK;
}

int dct13_xpr_receive_zmodem(struct Dct13Net *net,
                             Dct13XferStatusFunc status_func,
                             void *status_user)
{
    return dct13_xpr_receive_zmodem_prefixed(net, status_func, status_user, 0, 0);
}
