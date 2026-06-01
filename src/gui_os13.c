#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

#include "config.h"
#include "gui_os13.h"
#include "net.h"
#include "telnet.h"
#include "terminal.h"
#include "xfer_xpr.h"

#define TITLE "MiniTelnet v0.17 by Marcel Jaehne (c)2026"
#define RX_SIZE 240
#define TERM_SIZE 240
#define IAC_REPLY_SIZE 96

#define TERMINAL_MARGIN 2
#define CONN_LIST_X 12
#define CONN_LIST_Y 26
#define CONN_LIST_W 170
#define CONN_LIST_H 70
#define CONN_VISIBLE 7
#define CONN_ROW_H 10
#define CONN_HOST_X 236
#define CONN_HOST_Y 30
#define CONN_HOST_W 170
#define CONN_PORT_X 236
#define CONN_PORT_Y 54
#define CONN_PORT_W 60
#define CONN_STRING_H 12
#define CONN_BUTTON_Y 118
#define ADDR_BOOK_FILE "minitelnet.addr"
#define ADDR_BOOK_MAX 32
#define ADDR_NAME_SIZE 32

#define CGID_HOST 1
#define CGID_PORT 2
#define CGID_CONNECT 3
#define CGID_CANCEL 4
#define CGID_SAVE 5
#define CGID_DELETE 6
#define CGID_UP 7
#define CGID_DOWN 8
#define IGID_OK 201
#define FGID_OK 102
#define FGID_CANCEL 103
#define FGID_UP 104
#define FGID_DOWN 105

#define FONT_MAX 64
#define FONT_NAME_MAX 64
#define FONT_SIZE_MAX 16
#define FONT_VISIBLE 10
#define FONT_ROW_HEIGHT 10
#define FONT_LIST_X 12
#define FONT_LIST_Y 24
#define FONT_LIST_W 150
#define FONT_SIZE_X 174
#define FONT_SIZE_W 58
#define FONT_LIST_H (FONT_VISIBLE * FONT_ROW_HEIGHT)

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static struct Window *g_win;
static struct Dct13Config g_cfg;
static struct Dct13Net g_net;
static struct Dct13Telnet g_telnet;
static struct Dct13Terminal g_term;
static UBYTE g_rx[RX_SIZE];
static UBYTE g_term_buf[TERM_SIZE];
static UBYTE g_iac_reply[IAC_REPLY_SIZE];
static char g_host_undo[DCT13_HOST_SIZE];
static char g_port_undo[DCT13_PORT_SIZE];
static char g_status[96];
static char g_title_status[160];
static char g_addr_io_buf[512];
static char g_font_names[FONT_MAX][FONT_NAME_MAX];
static UWORD g_font_count;
static UWORD g_font_selected;
static UWORD g_font_top;
static UWORD g_font_sizes[FONT_SIZE_MAX];
static UWORD g_font_size_count;
static UWORD g_font_size_selected;
struct AddrBookEntry {
    char name[ADDR_NAME_SIZE];
    char host[DCT13_HOST_SIZE];
    char port[DCT13_PORT_SIZE];
};
static struct AddrBookEntry g_addr_book[ADDR_BOOK_MAX];
static UWORD g_addr_count;
static UWORD g_addr_selected;
static UWORD g_addr_top;
static int g_running;

static void resize_terminal(void);
static void redraw(void);
static void copy_cfg_text(char *dst, UWORD max, const char *src);

static struct StringInfo g_host_si = {
    (STRPTR)g_cfg.host, (STRPTR)g_host_undo, 0, DCT13_HOST_SIZE,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};
static struct StringInfo g_port_si = {
    (STRPTR)g_cfg.port_text, (STRPTR)g_port_undo, 0, DCT13_PORT_SIZE,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

static struct IntuiText g_connect_ok_text = { 0, 1, JAM2, 7, 1, 0, (UBYTE *)"Connect", 0 };
static struct IntuiText g_connect_save_text = { 0, 1, JAM2, 12, 1, 0, (UBYTE *)"Save", 0 };
static struct IntuiText g_connect_delete_text = { 0, 1, JAM2, 7, 1, 0, (UBYTE *)"Delete", 0 };
static struct IntuiText g_connect_up_text = { 0, 1, JAM2, 5, 1, 0, (UBYTE *)"Up", 0 };
static struct IntuiText g_connect_down_text = { 0, 1, JAM2, 3, 1, 0, (UBYTE *)"Down", 0 };
static struct IntuiText g_connect_cancel_text = { 0, 1, JAM2, 8, 1, 0, (UBYTE *)"Cancel", 0 };

static struct Gadget g_connect_cancel_gad = {
    0, 344, CONN_BUTTON_Y, 62, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_connect_cancel_text, 0, 0, CGID_CANCEL, 0
};
static struct Gadget g_connect_delete_gad = {
    &g_connect_cancel_gad, 270, CONN_BUTTON_Y, 62, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_connect_delete_text, 0, 0, CGID_DELETE, 0
};
static struct Gadget g_connect_save_gad = {
    &g_connect_delete_gad, 208, CONN_BUTTON_Y, 52, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_connect_save_text, 0, 0, CGID_SAVE, 0
};
static struct Gadget g_connect_ok_gad = {
    &g_connect_save_gad, 126, CONN_BUTTON_Y, 70, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_connect_ok_text, 0, 0, CGID_CONNECT, 0
};
static struct Gadget g_connect_down_gad = {
    &g_connect_ok_gad, 68, CONN_BUTTON_Y, 46, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_connect_down_text, 0, 0, CGID_DOWN, 0
};
static struct Gadget g_connect_up_gad = {
    &g_connect_down_gad, 12, CONN_BUTTON_Y, 46, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_connect_up_text, 0, 0, CGID_UP, 0
};
static struct Gadget g_conn_port_gad = {
    &g_connect_up_gad, CONN_PORT_X, CONN_PORT_Y, CONN_PORT_W, CONN_STRING_H,
    GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGLEFT,
    GTYP_STRGADGET, 0, 0, 0, 0, (APTR)&g_port_si, CGID_PORT, 0
};
static struct Gadget g_conn_host_gad = {
    &g_conn_port_gad, CONN_HOST_X, CONN_HOST_Y, CONN_HOST_W, CONN_STRING_H,
    GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_STRINGLEFT,
    GTYP_STRGADGET, 0, 0, 0, 0, (APTR)&g_host_si, CGID_HOST, 0
};

static struct IntuiText g_menu_connect_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Connect", 0 };
static struct IntuiText g_menu_hangup_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Hangup", 0 };
static struct IntuiText g_menu_clear_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Clear", 0 };
static struct IntuiText g_menu_zdownload_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"ZModem Download", 0 };
static struct IntuiText g_menu_info_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Info", 0 };
static struct IntuiText g_menu_quit_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Quit", 0 };
static struct IntuiText g_menu_font_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Terminal Font...", 0 };
static struct IntuiText g_menu_save_text = { 0, 1, JAM2, 6, 1, 0, (UBYTE *)"Save Settings", 0 };

static struct MenuItem g_project_quit_item = {
    0, 0, 50, 132, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_quit_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_project_info_item = {
    &g_project_quit_item, 0, 40, 132, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_info_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_project_zdownload_item = {
    &g_project_info_item, 0, 30, 132, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_zdownload_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_project_clear_item = {
    &g_project_zdownload_item, 0, 20, 132, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_clear_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_project_hangup_item = {
    &g_project_clear_item, 0, 10, 132, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_hangup_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_project_connect_item = {
    &g_project_hangup_item, 0, 0, 132, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_connect_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_settings_save_item = {
    0, 0, 10, 150, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_save_text, 0, 0, 0, MENUNULL
};
static struct MenuItem g_settings_font_item = {
    &g_settings_save_item, 0, 0, 150, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP, 0,
    (APTR)&g_menu_font_text, 0, 0, 0, MENUNULL
};

static struct Menu g_settings_menu = {
    0, 66, 0, 72, 10, MENUENABLED, (CONST_STRPTR)"Settings", &g_settings_font_item,
    0, 0, 0, 0
};
static struct Menu g_project_menu = {
    &g_settings_menu, 0, 0, 62, 10, MENUENABLED, (CONST_STRPTR)"Project", &g_project_connect_item,
    0, 0, 0, 0
};

static struct IntuiText g_font_ok_text = { 0, 1, JAM2, 16, 1, 0, (UBYTE *)"OK", 0 };
static struct IntuiText g_font_cancel_text = { 0, 1, JAM2, 8, 1, 0, (UBYTE *)"Cancel", 0 };
static struct IntuiText g_font_up_text = { 0, 1, JAM2, 5, 1, 0, (UBYTE *)"Up", 0 };
static struct IntuiText g_font_down_text = { 0, 1, JAM2, 3, 1, 0, (UBYTE *)"Down", 0 };
static struct IntuiText g_info_ok_text = { 0, 1, JAM2, 16, 1, 0, (UBYTE *)"OK", 0 };

static struct Gadget g_info_ok_gad = {
    0, 249, 100, 42, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_info_ok_text, 0, 0, IGID_OK, 0
};

static struct Gadget g_font_cancel_gad = {
    0, 160, 150, 62, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_font_cancel_text, 0, 0, FGID_CANCEL, 0
};
static struct Gadget g_font_ok_gad = {
    &g_font_cancel_gad, 110, 150, 42, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_font_ok_text, 0, 0, FGID_OK, 0
};
static struct Gadget g_font_down_gad = {
    &g_font_ok_gad, 238, 68, 44, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_font_down_text, 0, 0, FGID_DOWN, 0
};
static struct Gadget g_font_up_gad = {
    &g_font_down_gad, 238, 48, 44, 14, GFLG_GADGHCOMP, GACT_RELVERIFY,
    GTYP_BOOLGADGET, 0, 0, &g_font_up_text, 0, 0, FGID_UP, 0
};

static struct NewWindow g_new_window = {
    0, 0, 639, 210,
    0, 1,
    IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE |
    IDCMP_VANILLAKEY | IDCMP_MENUPICK,
    WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_SIZEGADGET |
    WFLG_SIZEBRIGHT | WFLG_SIZEBBOTTOM | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
    0,
    0,
    (STRPTR)TITLE,
    0,
    0,
    420, 150, 1000, 600,
    WBENCHSCREEN
};

static void set_initial_window_size(void)
{
    struct Screen *screen;
    WORD width;
    WORD height;

    screen = 0;
    if (IntuitionBase) {
        screen = IntuitionBase->ActiveScreen;
        if (!screen)
            screen = IntuitionBase->FirstScreen;
    }
    if (!screen)
        return;

    width = screen->Width;
    height = screen->Height;
    if (width < g_new_window.MinWidth)
        width = g_new_window.MinWidth;
    if (height < g_new_window.MinHeight)
        height = g_new_window.MinHeight;

    g_new_window.LeftEdge = 0;
    g_new_window.TopEdge = 0;
    g_new_window.Width = width;
    g_new_window.Height = height;
    g_new_window.MaxWidth = width;
    g_new_window.MaxHeight = height;
}

static UWORD text_len(const char *text)
{
    UWORD len;

    len = 0;
    while (text && text[len])
        ++len;
    return len;
}

static void update_window_title(void)
{
    UWORD i;
    UWORD j;

    if (!g_win)
        return;
    i = 0;
    for (j = 0; TITLE[j] && ((ULONG)i + 1UL) < sizeof(g_title_status); ++j)
        g_title_status[i++] = TITLE[j];
    if (g_status[0]) {
        if (((ULONG)i + 4UL) < sizeof(g_title_status)) {
            g_title_status[i++] = ' ';
            g_title_status[i++] = '-';
            g_title_status[i++] = ' ';
        }
        for (j = 0; g_status[j] && ((ULONG)i + 1UL) < sizeof(g_title_status); ++j)
            g_title_status[i++] = g_status[j];
    }
    g_title_status[i] = 0;
    SetWindowTitles(g_win, (STRPTR)g_title_status, (STRPTR)-1);
}

static void copy_status(const char *text)
{
    UWORD i;

    i = 0;
    while (text && text[i] && ((ULONG)i + 1UL) < sizeof(g_status)) {
        g_status[i] = text[i];
        ++i;
    }
    g_status[i] = 0;
    update_window_title();
}

static void copy_status_errno(const char *prefix, int err)
{
    char text[96];
    char digits[16];
    UWORD i;
    UWORD j;
    int neg;
    ULONG value;

    i = 0;
    while (prefix && prefix[i] && ((ULONG)i + 1UL) < sizeof(text)) {
        text[i] = prefix[i];
        ++i;
    }
    if (((ULONG)i + 8UL) < sizeof(text)) {
        text[i++] = ' ';
        text[i++] = 'e';
        text[i++] = 'r';
        text[i++] = 'r';
        text[i++] = 'n';
        text[i++] = 'o';
        text[i++] = '=';
    }

    neg = err < 0;
    value = neg ? (ULONG)(-err) : (ULONG)err;
    j = sizeof(digits) - 1;
    digits[j] = 0;
    do {
        --j;
        digits[j] = (char)('0' + (value % 10UL));
        value /= 10UL;
    } while (value && j > 0);
    if (neg && j > 0)
        digits[--j] = '-';

    while (digits[j] && ((ULONG)i + 1UL) < sizeof(text))
        text[i++] = digits[j++];
    text[i] = 0;
    copy_status(text);
}

static void append_uword(char *text, UWORD *pos, ULONG max, UWORD value)
{
    char digits[8];
    UWORD i;

    i = sizeof(digits) - 1;
    digits[i] = 0;
    do {
        --i;
        digits[i] = (char)('0' + (value % 10U));
        value = (UWORD)(value / 10U);
    } while (value && i > 0);
    while (digits[i] && ((ULONG)(*pos) + 1UL) < max)
        text[(*pos)++] = digits[i++];
}

static void copy_status_font(const char *name, UWORD size)
{
    char text[96];
    UWORD i;
    UWORD j;

    i = 0;
    for (j = 0; "Font changed: "[j] && ((ULONG)i + 1UL) < sizeof(text); ++j)
        text[i++] = "Font changed: "[j];
    for (j = 0; name && name[j] && ((ULONG)i + 1UL) < sizeof(text); ++j)
        text[i++] = name[j];
    if (((ULONG)i + 2UL) < sizeof(text))
        text[i++] = '/';
    append_uword(text, &i, sizeof(text), size);
    text[i] = 0;
    copy_status(text);
}

static char lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c + ('a' - 'A'));
    return c;
}

static int ends_with_font(const char *name)
{
    UWORD len;
    const char *suffix;
    UWORD i;

    suffix = ".font";
    len = 0;
    while (name && name[len])
        ++len;
    if (len < 5)
        return 0;
    name += len - 5;
    for (i = 0; suffix[i]; ++i) {
        if (lower_ascii(name[i]) != suffix[i])
            return 0;
    }
    return 1;
}

static int same_text(const char *a, const char *b)
{
    UWORD i;

    i = 0;
    while (a && b && a[i] && b[i]) {
        if (lower_ascii(a[i]) != lower_ascii(b[i]))
            return 0;
        ++i;
    }
    return a && b && a[i] == 0 && b[i] == 0;
}

static void copy_font_entry(char *dst, const char *src)
{
    UWORD i;

    i = 0;
    while (src && src[i] && ((ULONG)i + 1UL) < FONT_NAME_MAX) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void scan_fonts(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;

    g_font_count = 0;
    g_font_selected = 0;
    g_font_top = 0;
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib)
        return;
    lock = Lock((STRPTR)"FONTS:", ACCESS_READ);
    if (lock) {
        if (Examine(lock, fib)) {
            while (ExNext(lock, fib) && g_font_count < FONT_MAX) {
                if (fib->fib_DirEntryType < 0 && ends_with_font((const char *)fib->fib_FileName)) {
                    copy_font_entry(g_font_names[g_font_count], (const char *)fib->fib_FileName);
                    if (same_text(g_font_names[g_font_count], dct13_term_font_name(&g_term)))
                        g_font_selected = g_font_count;
                    ++g_font_count;
                }
            }
        }
        UnLock(lock);
    }
    FreeMem(fib, sizeof(struct FileInfoBlock));
}

static int parse_numeric_name(const char *name, UWORD *out)
{
    UWORD i;
    ULONG value;

    if (!name || !name[0])
        return 0;
    i = 0;
    value = 0;
    while (name[i]) {
        if (name[i] < '0' || name[i] > '9')
            return 0;
        value = value * 10UL + (ULONG)(name[i] - '0');
        if (value > 32UL)
            return 0;
        ++i;
    }
    if (value < 4UL)
        return 0;
    *out = (UWORD)value;
    return 1;
}

static void strip_font_suffix(const char *font_name, char *base, ULONG base_size)
{
    UWORD len;
    UWORD copy_len;
    UWORD i;

    if (!base || base_size == 0)
        return;
    len = 0;
    while (font_name && font_name[len])
        ++len;
    copy_len = len;
    if (len > 5 && ends_with_font(font_name))
        copy_len = (UWORD)(len - 5);
    if ((ULONG)copy_len >= base_size)
        copy_len = (UWORD)(base_size - 1);
    for (i = 0; i < copy_len; ++i)
        base[i] = font_name[i];
    base[copy_len] = 0;
}

static void append_text(char *dst, ULONG dst_size, UWORD *pos, const char *src)
{
    UWORD i;

    i = 0;
    while (src && src[i] && ((ULONG)(*pos) + 1UL) < dst_size)
        dst[(*pos)++] = src[i++];
    dst[*pos] = 0;
}

static void make_font_size_path(const char *font_name, char *path, ULONG path_size)
{
    char base[FONT_NAME_MAX];
    UWORD pos;

    strip_font_suffix(font_name, base, sizeof(base));
    pos = 0;
    append_text(path, path_size, &pos, "FONTS:");
    append_text(path, path_size, &pos, base);
}

static void add_font_size(UWORD size)
{
    UWORD i;

    for (i = 0; i < g_font_size_count; ++i) {
        if (g_font_sizes[i] == size)
            return;
    }
    if (g_font_size_count < FONT_SIZE_MAX)
        g_font_sizes[g_font_size_count++] = size;
}

static void scan_font_sizes(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    char path[96];
    UWORD size;
    UWORD current_size;

    g_font_size_count = 0;
    g_font_size_selected = 0;
    current_size = dct13_term_font_size(&g_term);
    if (g_font_count == 0)
        return;
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib)
        return;
    make_font_size_path(g_font_names[g_font_selected], path, sizeof(path));
    lock = Lock((STRPTR)path, ACCESS_READ);
    if (lock) {
        if (Examine(lock, fib)) {
            while (ExNext(lock, fib) && g_font_size_count < FONT_SIZE_MAX) {
                if (fib->fib_DirEntryType < 0 && parse_numeric_name((const char *)fib->fib_FileName, &size))
                    add_font_size(size);
            }
        }
        UnLock(lock);
    }
    FreeMem(fib, sizeof(struct FileInfoBlock));
    if (g_font_size_count == 0) {
        if (current_size == 0)
            current_size = 8;
        add_font_size(current_size);
    }
    for (size = 0; size < g_font_size_count; ++size) {
        if (g_font_sizes[size] == current_size) {
            g_font_size_selected = size;
            break;
        }
    }
}

static void draw_box(struct Window *win, WORD x, WORD y, WORD w, WORD h)
{
    Move(win->RPort, x, y);
    Draw(win->RPort, (WORD)(x + w - 1), y);
    Draw(win->RPort, (WORD)(x + w - 1), (WORD)(y + h - 1));
    Draw(win->RPort, x, (WORD)(y + h - 1));
    Draw(win->RPort, x, y);
}

static void draw_button_box(struct Window *win, struct Gadget *gad, const char *label, WORD text_x)
{
    draw_box(win, gad->LeftEdge, gad->TopEdge, gad->Width, gad->Height);
    Move(win->RPort, (WORD)(gad->LeftEdge + text_x), (WORD)(gad->TopEdge + 10));
    Text(win->RPort, (STRPTR)label, text_len(label));
}

static void number_to_text(UWORD value, char *dst, ULONG dst_size)
{
    UWORD pos;

    pos = 0;
    append_uword(dst, &pos, dst_size, value);
    dst[pos] = 0;
}

static void draw_font_selector(struct Window *win)
{
    UWORD row;
    UWORD idx;
    WORD y;
    char line[FONT_NAME_MAX + 3];
    char num[8];
    UWORD i;
    UWORD j;

    if (!win)
        return;
    SetFont(win->RPort, g_term.gui_font);
    SetBPen(win->RPort, 1);
    SetDrMd(win->RPort, JAM1);
    SetAPen(win->RPort, 1);
    RectFill(win->RPort, 0, 10, (WORD)(win->Width - 1), (WORD)(win->Height - 1));
    SetAPen(win->RPort, 0);
    Move(win->RPort, FONT_LIST_X, 18);
    Text(win->RPort, (STRPTR)"Font", 4);
    Move(win->RPort, FONT_SIZE_X, 18);
    Text(win->RPort, (STRPTR)"Size", 4);
    draw_box(win, FONT_LIST_X, FONT_LIST_Y, FONT_LIST_W, FONT_LIST_H);
    draw_box(win, FONT_SIZE_X, FONT_LIST_Y, FONT_SIZE_W, FONT_LIST_H);
    draw_button_box(win, &g_font_up_gad, "Up", 5);
    draw_button_box(win, &g_font_down_gad, "Down", 3);
    draw_button_box(win, &g_font_ok_gad, "OK", 16);
    draw_button_box(win, &g_font_cancel_gad, "Cancel", 8);

    for (row = 0; row < FONT_VISIBLE; ++row) {
        idx = (UWORD)(g_font_top + row);
        if (idx >= g_font_count)
            break;
        line[0] = (idx == g_font_selected) ? '>' : ' ';
        line[1] = ' ';
        i = 2;
        for (j = 0; g_font_names[idx][j] && ((ULONG)i + 1UL) < sizeof(line); ++j)
            line[i++] = g_font_names[idx][j];
        line[i] = 0;
        y = (WORD)(FONT_LIST_Y + 10 + row * FONT_ROW_HEIGHT);
        Move(win->RPort, (WORD)(FONT_LIST_X + 3), y);
        Text(win->RPort, (STRPTR)line, i);
    }
    if (g_font_count == 0) {
        Move(win->RPort, (WORD)(FONT_LIST_X + 3), (WORD)(FONT_LIST_Y + 18));
        Text(win->RPort, (STRPTR)"No .font files", 14);
    }

    for (row = 0; row < g_font_size_count && row < FONT_VISIBLE; ++row) {
        line[0] = (row == g_font_size_selected) ? '>' : ' ';
        line[1] = ' ';
        number_to_text(g_font_sizes[row], num, sizeof(num));
        i = 2;
        for (j = 0; num[j] && ((ULONG)i + 1UL) < sizeof(line); ++j)
            line[i++] = num[j];
        line[i] = 0;
        y = (WORD)(FONT_LIST_Y + 10 + row * FONT_ROW_HEIGHT);
        Move(win->RPort, (WORD)(FONT_SIZE_X + 3), y);
        Text(win->RPort, (STRPTR)line, i);
    }
    if (g_font_count && g_font_size_count == 0) {
        Move(win->RPort, (WORD)(FONT_SIZE_X + 3), (WORD)(FONT_LIST_Y + 18));
        Text(win->RPort, (STRPTR)"none", 4);
    }
}

static void font_selector_apply(void)
{
    UWORD size;

    if (g_font_count == 0) {
        copy_status("No fonts found");
        return;
    }
    if (g_font_size_count == 0) {
        copy_status("No font sizes found");
        return;
    }
    size = g_font_sizes[g_font_size_selected];
    if (!dct13_term_apply_font(&g_term, g_font_names[g_font_selected], size)) {
        copy_status("Font not available");
        return;
    }
    resize_terminal();
    copy_status_font(g_font_names[g_font_selected], size);
}

static void open_font_selector(void)
{
    struct Window *fw;
    struct NewWindow nw;
    struct IntuiMessage *msg;
    ULONG cls;
    UWORD code;
    APTR addr;
    WORD mx;
    WORD my;
    int done;
    UWORD row;

    dct13_term_restore_gui_rp(&g_term);
    scan_fonts();
    scan_font_sizes();
    nw.LeftEdge = 90;
    nw.TopEdge = 35;
    nw.Width = 292;
    nw.Height = 178;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_GADGETUP | IDCMP_MOUSEBUTTONS;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
        WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = &g_font_up_gad;
    nw.CheckMark = 0;
    nw.Title = (STRPTR)"Terminal Font";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = 292;
    nw.MinHeight = 178;
    nw.MaxWidth = 292;
    nw.MaxHeight = 178;
    nw.Type = WBENCHSCREEN;
    fw = OpenWindow(&nw);
    if (!fw) {
        copy_status("Font requester failed");
        return;
    }
    draw_font_selector(fw);
    done = 0;
    while (!done) {
        WaitPort(fw->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(fw->UserPort)) != 0) {
            cls = msg->Class;
            code = msg->Code;
            addr = msg->IAddress;
            mx = msg->MouseX;
            my = msg->MouseY;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(fw);
                draw_font_selector(fw);
                EndRefresh(fw, TRUE);
            } else if (cls == IDCMP_GADGETUP) {
                if (((struct Gadget *)addr)->GadgetID == FGID_CANCEL) {
                    done = 1;
                } else if (((struct Gadget *)addr)->GadgetID == FGID_OK) {
                    font_selector_apply();
                    done = 1;
                } else if (((struct Gadget *)addr)->GadgetID == FGID_UP) {
                    if (g_font_top > 0)
                        --g_font_top;
                    draw_font_selector(fw);
                } else if (((struct Gadget *)addr)->GadgetID == FGID_DOWN) {
                    if ((ULONG)g_font_top + FONT_VISIBLE < g_font_count)
                        ++g_font_top;
                    draw_font_selector(fw);
                }
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                if (mx >= FONT_LIST_X && mx < (FONT_LIST_X + FONT_LIST_W) &&
                    my >= FONT_LIST_Y && my < (FONT_LIST_Y + FONT_LIST_H)) {
                    row = (UWORD)((my - FONT_LIST_Y) / FONT_ROW_HEIGHT);
                    if ((ULONG)g_font_top + row < g_font_count) {
                        g_font_selected = (UWORD)(g_font_top + row);
                        scan_font_sizes();
                        draw_font_selector(fw);
                    }
                } else if (mx >= FONT_SIZE_X && mx < (FONT_SIZE_X + FONT_SIZE_W) &&
                    my >= FONT_LIST_Y && my < (FONT_LIST_Y + FONT_LIST_H)) {
                    row = (UWORD)((my - FONT_LIST_Y) / FONT_ROW_HEIGHT);
                    if (row < g_font_size_count) {
                        g_font_size_selected = row;
                        draw_font_selector(fw);
                    }
                }
            }
        }
    }
    CloseWindow(fw);
    redraw();
}

static void draw_info_dialog(struct Window *win)
{
    if (!win)
        return;
    SetFont(win->RPort, g_term.gui_font);
    SetBPen(win->RPort, 1);
    SetDrMd(win->RPort, JAM1);
    SetAPen(win->RPort, 1);
    RectFill(win->RPort, 0, 10, (WORD)(win->Width - 1), (WORD)(win->Height - 1));
    SetAPen(win->RPort, 0);
    Move(win->RPort, 14, 25);
    Text(win->RPort, (STRPTR)"MiniTelnet for Kick1.3", text_len("MiniTelnet for Kick1.3"));
    Move(win->RPort, 14, 39);
    Text(win->RPort, (STRPTR)"Version: v0.17", text_len("Version: v0.17"));
    Move(win->RPort, 14, 53);
    Text(win->RPort, (STRPTR)"by Marcel Jaehne", text_len("by Marcel Jaehne"));
    Move(win->RPort, 14, 67);
    Text(win->RPort, (STRPTR)"(c) 2026", text_len("(c) 2026"));
    Move(win->RPort, 14, 81);
    Text(win->RPort, (STRPTR)"If you want to buy me a coffe, send me a buck to:",
        text_len("If you want to buy me a coffe, send me a buck to:"));
    Move(win->RPort, 14, 95);
    Text(win->RPort, (STRPTR)"https://paypal.me/mytubefree",
        text_len("https://paypal.me/mytubefree"));
    draw_button_box(win, &g_info_ok_gad, "OK", 16);
}

static void open_info_dialog(void)
{
    struct Window *iw;
    struct NewWindow nw;
    struct IntuiMessage *msg;
    ULONG cls;
    APTR addr;
    int done;

    dct13_term_restore_gui_rp(&g_term);
    nw.LeftEdge = 45;
    nw.TopEdge = 40;
    nw.Width = 540;
    nw.Height = 130;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_GADGETUP;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
        WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = &g_info_ok_gad;
    nw.CheckMark = 0;
    nw.Title = (STRPTR)"MiniTelnet Info";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = 540;
    nw.MinHeight = 130;
    nw.MaxWidth = 540;
    nw.MaxHeight = 130;
    nw.Type = WBENCHSCREEN;
    iw = OpenWindow(&nw);
    if (!iw) {
        copy_status("Info window failed");
        return;
    }
    draw_info_dialog(iw);
    done = 0;
    while (!done) {
        WaitPort(iw->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(iw->UserPort)) != 0) {
            cls = msg->Class;
            addr = msg->IAddress;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(iw);
                draw_info_dialog(iw);
                EndRefresh(iw, TRUE);
            } else if (cls == IDCMP_GADGETUP) {
                if (((struct Gadget *)addr)->GadgetID == IGID_OK)
                    done = 1;
            }
        }
    }
    CloseWindow(iw);
    redraw();
}


static void addr_book_clear(void)
{
    g_addr_count = 0;
    g_addr_selected = 0;
    g_addr_top = 0;
}

static void addr_book_copy_name(char *dst, const char *src)
{
    UWORD i;

    i = 0;
    while (src && src[i] && ((ULONG)i + 1UL) < ADDR_NAME_SIZE) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void addr_book_add(const char *name, const char *host, const char *port)
{
    UWORD idx;

    if (!host || !host[0] || g_addr_count >= ADDR_BOOK_MAX)
        return;
    idx = g_addr_count++;
    if (name && name[0])
        addr_book_copy_name(g_addr_book[idx].name, name);
    else
        addr_book_copy_name(g_addr_book[idx].name, host);
    copy_cfg_text(g_addr_book[idx].host, DCT13_HOST_SIZE, host);
    if (port && port[0])
        copy_cfg_text(g_addr_book[idx].port, DCT13_PORT_SIZE, port);
    else
        copy_cfg_text(g_addr_book[idx].port, DCT13_PORT_SIZE, "23");
}

static void addr_book_parse_line(char *line)
{
    char *name;
    char *host;
    char *port;
    UWORD i;
    UWORD parsed_port;

    if (!line || !line[0] || line[0] == '#')
        return;
    name = line;
    host = 0;
    port = 0;
    for (i = 0; line[i]; ++i) {
        if (line[i] == '|') {
            line[i] = 0;
            host = &line[i + 1];
            break;
        }
    }
    if (!host || !host[0])
        return;
    for (i = 0; host[i]; ++i) {
        if (host[i] == '|') {
            host[i] = 0;
            port = &host[i + 1];
            break;
        }
    }
    if (!port || !port[0])
        port = "23";
    if (!dct13_parse_port(port, &parsed_port))
        return;
    addr_book_add(name, host, port);
}

static void addr_book_load(void)
{
    BPTR fh;
    LONG got;
    UWORD i;
    UWORD line_pos;
    char ch;
    static char line[160];

    addr_book_clear();
    fh = Open((STRPTR)ADDR_BOOK_FILE, MODE_OLDFILE);
    if (!fh)
        return;
    line_pos = 0;
    while ((got = Read(fh, g_addr_io_buf, sizeof(g_addr_io_buf))) > 0) {
        for (i = 0; i < (UWORD)got; ++i) {
            ch = g_addr_io_buf[i];
            if (ch == '\r' || ch == '\n') {
                if (line_pos) {
                    line[line_pos] = 0;
                    addr_book_parse_line(line);
                    line_pos = 0;
                }
            } else if (((ULONG)line_pos + 1UL) < sizeof(line)) {
                line[line_pos++] = ch;
            }
        }
    }
    if (line_pos) {
        line[line_pos] = 0;
        addr_book_parse_line(line);
    }
    Close(fh);
}

static int addr_book_write_text(BPTR fh, const char *text)
{
    UWORD len;

    len = text_len(text);
    if (len == 0)
        return 1;
    return Write(fh, (APTR)text, len) == len;
}

static int addr_book_save(void)
{
    BPTR fh;
    UWORD i;

    fh = Open((STRPTR)ADDR_BOOK_FILE, MODE_NEWFILE);
    if (!fh)
        return 0;
    if (!addr_book_write_text(fh, "# MiniTelnet address book\n")) {
        Close(fh);
        return 0;
    }
    for (i = 0; i < g_addr_count; ++i) {
        if (!addr_book_write_text(fh, g_addr_book[i].name) ||
            !addr_book_write_text(fh, "|") ||
            !addr_book_write_text(fh, g_addr_book[i].host) ||
            !addr_book_write_text(fh, "|") ||
            !addr_book_write_text(fh, g_addr_book[i].port) ||
            !addr_book_write_text(fh, "\n")) {
            Close(fh);
            return 0;
        }
    }
    Close(fh);
    return 1;
}

static void addr_book_select(struct Window *win, UWORD idx)
{
    if (idx >= g_addr_count)
        return;
    g_addr_selected = idx;
    copy_cfg_text(g_cfg.host, DCT13_HOST_SIZE, g_addr_book[idx].host);
    copy_cfg_text(g_cfg.port_text, DCT13_PORT_SIZE, g_addr_book[idx].port);
    if (win)
        RefreshGList(&g_conn_host_gad, win, 0, 2);
}

static void addr_book_save_current(void)
{
    UWORD port;
    UWORD idx;

    if (!g_cfg.host[0]) {
        copy_status("Enter host");
        return;
    }
    if (!dct13_parse_port(g_cfg.port_text, &port)) {
        copy_status("Invalid port");
        return;
    }
    idx = g_addr_count;
    if (g_addr_selected < g_addr_count &&
        same_text(g_addr_book[g_addr_selected].host, g_cfg.host))
        idx = g_addr_selected;
    if (idx >= g_addr_count) {
        if (g_addr_count >= ADDR_BOOK_MAX) {
            copy_status("Address book full");
            return;
        }
        idx = g_addr_count++;
        g_addr_selected = idx;
    }
    addr_book_copy_name(g_addr_book[idx].name, g_cfg.host);
    copy_cfg_text(g_addr_book[idx].host, DCT13_HOST_SIZE, g_cfg.host);
    copy_cfg_text(g_addr_book[idx].port, DCT13_PORT_SIZE, g_cfg.port_text[0] ? g_cfg.port_text : "23");
    if (idx < g_addr_top)
        g_addr_top = idx;
    if ((ULONG)idx >= (ULONG)g_addr_top + CONN_VISIBLE)
        g_addr_top = (UWORD)(idx - CONN_VISIBLE + 1);
    if (addr_book_save())
        copy_status("Address saved");
    else
        copy_status("Could not save address");
}

static void addr_book_delete_selected(void)
{
    UWORD i;

    if (g_addr_count == 0 || g_addr_selected >= g_addr_count)
        return;
    for (i = g_addr_selected; (ULONG)i + 1UL < g_addr_count; ++i)
        g_addr_book[i] = g_addr_book[i + 1];
    --g_addr_count;
    if (g_addr_selected >= g_addr_count && g_addr_selected > 0)
        --g_addr_selected;
    if (g_addr_top >= g_addr_count && g_addr_top > 0)
        --g_addr_top;
    if (addr_book_save())
        copy_status("Address deleted");
    else
        copy_status("Could not save address");
}

static void draw_addr_book_list(struct Window *win)
{
    UWORD row;
    UWORD idx;
    UWORD i;
    UWORD j;
    WORD y;
    char line[96];

    draw_box(win, CONN_LIST_X, CONN_LIST_Y, CONN_LIST_W, CONN_LIST_H);
    for (row = 0; row < CONN_VISIBLE; ++row) {
        idx = (UWORD)(g_addr_top + row);
        if (idx >= g_addr_count)
            break;
        line[0] = (idx == g_addr_selected) ? '>' : ' ';
        line[1] = ' ';
        i = 2;
        for (j = 0; g_addr_book[idx].name[j] && ((ULONG)i + 1UL) < sizeof(line); ++j)
            line[i++] = g_addr_book[idx].name[j];
        if (((ULONG)i + 2UL) < sizeof(line)) {
            line[i++] = ':';
            line[i++] = ' ';
        }
        for (j = 0; g_addr_book[idx].port[j] && ((ULONG)i + 1UL) < sizeof(line); ++j)
            line[i++] = g_addr_book[idx].port[j];
        line[i] = 0;
        y = (WORD)(CONN_LIST_Y + 10 + row * CONN_ROW_H);
        Move(win->RPort, (WORD)(CONN_LIST_X + 3), y);
        Text(win->RPort, (STRPTR)line, i);
    }
    if (g_addr_count == 0) {
        Move(win->RPort, (WORD)(CONN_LIST_X + 3), (WORD)(CONN_LIST_Y + 18));
        Text(win->RPort, (STRPTR)"No entries", 10);
    }
}

static void draw_connect_dialog(struct Window *win)
{
    if (!win)
        return;
    SetFont(win->RPort, g_term.gui_font);
    SetBPen(win->RPort, 1);
    SetDrMd(win->RPort, JAM1);
    SetAPen(win->RPort, 1);
    RectFill(win->RPort, 0, 10, (WORD)(win->Width - 1), (WORD)(win->Height - 1));
    SetAPen(win->RPort, 0);
    Move(win->RPort, CONN_LIST_X, 20);
    Text(win->RPort, (STRPTR)"Address book", 12);
    draw_addr_book_list(win);
    Move(win->RPort, 196, 40);
    Text(win->RPort, (STRPTR)"Host", 4);
    Move(win->RPort, 196, 64);
    Text(win->RPort, (STRPTR)"Port", 4);
    draw_box(win, CONN_HOST_X, CONN_HOST_Y, CONN_HOST_W, CONN_STRING_H);
    draw_box(win, CONN_PORT_X, CONN_PORT_Y, CONN_PORT_W, CONN_STRING_H);
    draw_button_box(win, &g_connect_up_gad, "Up", 5);
    draw_button_box(win, &g_connect_down_gad, "Down", 3);
    draw_button_box(win, &g_connect_ok_gad, "Connect", 7);
    draw_button_box(win, &g_connect_save_gad, "Save", 12);
    draw_button_box(win, &g_connect_delete_gad, "Delete", 7);
    draw_button_box(win, &g_connect_cancel_gad, "Cancel", 8);
    RefreshGList(&g_conn_host_gad, win, 0, 2);
}

static void restore_connect_text(const char *old_host, const char *old_port)
{
    copy_cfg_text(g_cfg.host, DCT13_HOST_SIZE, old_host);
    copy_cfg_text(g_cfg.port_text, DCT13_PORT_SIZE, old_port);
}

static int open_connect_dialog(void)
{
    struct Window *cw;
    struct NewWindow nw;
    struct IntuiMessage *msg;
    ULONG cls;
    UWORD code;
    APTR addr;
    WORD mx;
    WORD my;
    UWORD row;
    int done;
    int accepted;
    char old_host[DCT13_HOST_SIZE];
    char old_port[DCT13_PORT_SIZE];

    copy_cfg_text(old_host, DCT13_HOST_SIZE, g_cfg.host);
    copy_cfg_text(old_port, DCT13_PORT_SIZE, g_cfg.port_text);
    dct13_term_restore_gui_rp(&g_term);
    addr_book_load();
    nw.LeftEdge = 70;
    nw.TopEdge = 35;
    nw.Width = 424;
    nw.Height = 152;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_GADGETUP | IDCMP_MOUSEBUTTONS;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
        WFLG_ACTIVATE | WFLG_SMART_REFRESH;
    nw.FirstGadget = &g_conn_host_gad;
    nw.CheckMark = 0;
    nw.Title = (STRPTR)"Connect";
    nw.Screen = 0;
    nw.BitMap = 0;
    nw.MinWidth = 424;
    nw.MinHeight = 152;
    nw.MaxWidth = 424;
    nw.MaxHeight = 152;
    nw.Type = WBENCHSCREEN;
    cw = OpenWindow(&nw);
    if (!cw) {
        copy_status("Connect window failed");
        return 0;
    }
    draw_connect_dialog(cw);
    ActivateGadget(&g_conn_host_gad, cw, 0);
    done = 0;
    accepted = 0;
    while (!done) {
        WaitPort(cw->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(cw->UserPort)) != 0) {
            cls = msg->Class;
            code = msg->Code;
            addr = msg->IAddress;
            mx = msg->MouseX;
            my = msg->MouseY;
            ReplyMsg((struct Message *)msg);
            if (cls == IDCMP_CLOSEWINDOW) {
                done = 1;
            } else if (cls == IDCMP_REFRESHWINDOW) {
                BeginRefresh(cw);
                draw_connect_dialog(cw);
                EndRefresh(cw, TRUE);
            } else if (cls == IDCMP_MOUSEBUTTONS && code == SELECTDOWN) {
                if (mx >= CONN_LIST_X && mx < (CONN_LIST_X + CONN_LIST_W) &&
                    my >= CONN_LIST_Y && my < (CONN_LIST_Y + CONN_LIST_H)) {
                    row = (UWORD)((my - CONN_LIST_Y) / CONN_ROW_H);
                    if ((ULONG)g_addr_top + row < g_addr_count) {
                        addr_book_select(cw, (UWORD)(g_addr_top + row));
                        draw_connect_dialog(cw);
                    }
                }
            } else if (cls == IDCMP_GADGETUP) {
                if (((struct Gadget *)addr)->GadgetID == CGID_CANCEL) {
                    done = 1;
                } else if (((struct Gadget *)addr)->GadgetID == CGID_CONNECT) {
                    accepted = 1;
                    done = 1;
                } else if (((struct Gadget *)addr)->GadgetID == CGID_SAVE) {
                    addr_book_save_current();
                    draw_connect_dialog(cw);
                } else if (((struct Gadget *)addr)->GadgetID == CGID_DELETE) {
                    addr_book_delete_selected();
                    draw_connect_dialog(cw);
                } else if (((struct Gadget *)addr)->GadgetID == CGID_UP) {
                    if (g_addr_top > 0)
                        --g_addr_top;
                    draw_connect_dialog(cw);
                } else if (((struct Gadget *)addr)->GadgetID == CGID_DOWN) {
                    if ((ULONG)g_addr_top + CONN_VISIBLE < g_addr_count)
                        ++g_addr_top;
                    draw_connect_dialog(cw);
                }
            }
        }
    }
    CloseWindow(cw);
    if (!accepted)
        restore_connect_text(old_host, old_port);
    redraw();
    return accepted;
}


static void resize_terminal(void)
{
    WORD left;
    WORD top;
    WORD width;
    WORD height;

    left = (WORD)(g_win->BorderLeft + TERMINAL_MARGIN);
    top = (WORD)(g_win->BorderTop + TERMINAL_MARGIN);
    width = (WORD)(g_win->Width - g_win->BorderLeft - g_win->BorderRight - (TERMINAL_MARGIN * 2));
    height = (WORD)(g_win->Height - g_win->BorderTop - g_win->BorderBottom - (TERMINAL_MARGIN * 2));
    if (width < 160)
        width = 160;
    if (height < 40)
        height = 40;
    dct13_term_resize(&g_term, left, top, width, height);
}

static void redraw(void)
{
    BeginRefresh(g_win);
    dct13_term_redraw(&g_term);
    EndRefresh(g_win, TRUE);
    update_window_title();
}

static int open_libs(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    return IntuitionBase && GfxBase;
}

static void close_libs(void)
{
    if (GfxBase)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
}

static int open_window(void)
{
    set_initial_window_size();
    g_win = OpenWindow(&g_new_window);
    if (!g_win)
        return 0;
    if (!dct13_term_init(&g_term, g_win,
        (WORD)(g_win->BorderLeft + TERMINAL_MARGIN),
        (WORD)(g_win->BorderTop + TERMINAL_MARGIN),
        (WORD)(g_win->Width - g_win->BorderLeft - g_win->BorderRight - (TERMINAL_MARGIN * 2)),
        (WORD)(g_win->Height - g_win->BorderTop - g_win->BorderBottom - (TERMINAL_MARGIN * 2)))) {
        CloseWindow(g_win);
        g_win = 0;
        return 0;
    }
    if (g_cfg.font[0] && g_cfg.font_size)
        dct13_term_apply_font(&g_term, g_cfg.font, g_cfg.font_size);
    dct13_term_set_mode(&g_term, g_cfg.terminal_mode);
    SetMenuStrip(g_win, &g_project_menu);
    dct13_term_clear(&g_term);
    copy_status("Ready");
    return 1;
}

static void close_window(void)
{
    if (g_win) {
        ClearMenuStrip(g_win);
        dct13_term_free(&g_term);
        CloseWindow(g_win);
        g_win = 0;
    }
}

static void copy_cfg_text(char *dst, UWORD max, const char *src)
{
    UWORD i;

    if (!dst || max == 0)
        return;
    i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void save_settings(void)
{
    UWORD port;

    if (!dct13_parse_port(g_cfg.port_text, &port)) {
        copy_status("Invalid port");
        return;
    }
    g_cfg.port = port;
    if (!g_cfg.port_text[0])
        copy_cfg_text(g_cfg.port_text, DCT13_PORT_SIZE, "23");
    copy_cfg_text(g_cfg.font, DCT13_FONT_NAME_SIZE, dct13_term_font_name(&g_term));
    g_cfg.font_size = dct13_term_font_size(&g_term);
    g_cfg.terminal_mode = g_term.mode;
    if (dct13_config_save(&g_cfg))
        copy_status("Settings saved");
    else
        copy_status("Could not save settings");
}

static void disconnect(void)
{
    if (dct13_net_connected(&g_net)) {
        dct13_net_close(&g_net);
        copy_status("Disconnected");
    }
}

static void connect_now(void)
{
    UWORD port;

    if (!dct13_parse_port(g_cfg.port_text, &port)) {
        copy_status("Invalid port");
        return;
    }
    if (!g_cfg.host[0]) {
        copy_status("Enter host");
        return;
    }
    g_cfg.port = port;
    copy_status("Connecting...");
    dct13_net_close(&g_net);
    dct13_telnet_init(&g_telnet);
    if (dct13_net_connect(&g_net, g_cfg.host, port, 20) == DCT13_NET_OK) {
        copy_status("Connected");
        dct13_term_write(&g_term, (const UBYTE *)"\r\n", 2);
    } else {
        if (g_net.last_errno == -1)
            copy_status("bsdsocket.library missing");
        else
            copy_status_errno("Connect failed", g_net.last_errno);
    }
}

static void send_key(UBYTE ch)
{
    if (!dct13_net_connected(&g_net))
        return;
    if (ch == 13)
        dct13_net_send(&g_net, (const UBYTE *)"\r\n", 2);
    else
        dct13_net_send(&g_net, &ch, 1);
}

static void poll_net(void)
{
    int r;
    UWORD packed;
    UWORD out_len;
    UWORD reply_len;

    if (!dct13_net_connected(&g_net))
        return;
    for (;;) {
        r = dct13_net_recv(&g_net, g_rx, sizeof(g_rx));
        if (r > 0) {
            packed = dct13_telnet_filter(&g_telnet, g_rx, (UWORD)r,
                g_term_buf, sizeof(g_term_buf), g_iac_reply, sizeof(g_iac_reply));
            out_len = (UWORD)(packed & 0xff);
            reply_len = (UWORD)(packed >> 8);
            if (reply_len)
                dct13_net_send(&g_net, g_iac_reply, reply_len);
            if (out_len)
                dct13_term_write(&g_term, g_term_buf, out_len);
            continue;
        }
        if (r == DCT13_NET_CLOSED) {
            copy_status("Remote closed");
        } else if (r == DCT13_NET_ERROR) {
            copy_status_errno("Socket error", g_net.last_errno);
        }
        break;
    }
}

static void clear_terminal_view(void)
{
    dct13_term_clear(&g_term);
}

static void xfer_status_callback(const char *text, void *user)
{
    (void)user;
    copy_status(text);
}

static void start_zmodem_download(void)
{
    int rc;

    if (!dct13_net_connected(&g_net)) {
        copy_status("Not connected");
        return;
    }
    copy_status("Starting ZModem download...");
    rc = dct13_xpr_receive_zmodem(&g_net, xfer_status_callback, 0);
    if (rc == DCT13_XFER_NO_LIBRARY)
        copy_status("xprzmodem.library missing");
    else if (rc == DCT13_XFER_NOT_CONNECTED)
        copy_status("Not connected");
    else if (rc != DCT13_XFER_OK)
        copy_status("ZModem download failed");
}

static void handle_menu(UWORD code)
{
    struct MenuItem *item;
    UWORD menu;
    UWORD item_no;

    while (code != MENUNULL) {
        item = ItemAddress(&g_project_menu, code);
        menu = MENUNUM(code);
        item_no = ITEMNUM(code);
        if (menu == 0) {
            if (item_no == 0) {
                if (open_connect_dialog())
                    connect_now();
            }
            else if (item_no == 1)
                disconnect();
            else if (item_no == 2)
                clear_terminal_view();
            else if (item_no == 3)
                start_zmodem_download();
            else if (item_no == 4)
                open_info_dialog();
            else if (item_no == 5)
                g_running = 0;
        } else if (menu == 1) {
            if (item_no == 0)
                open_font_selector();
            else if (item_no == 1)
                save_settings();
        }
        code = item ? item->NextSelect : MENUNULL;
    }
}

static void handle_window_messages(void)
{
    struct IntuiMessage *msg;
    ULONG cls;
    UWORD code;

    while ((msg = (struct IntuiMessage *)GetMsg(g_win->UserPort)) != 0) {
        cls = msg->Class;
        code = msg->Code;
        ReplyMsg((struct Message *)msg);
        if (cls == IDCMP_CLOSEWINDOW) {
            g_running = 0;
        } else if (cls == IDCMP_REFRESHWINDOW) {
            redraw();
        } else if (cls == IDCMP_NEWSIZE) {
            resize_terminal();
        } else if (cls == IDCMP_MENUPICK) {
            handle_menu(code);
        } else if (cls == IDCMP_VANILLAKEY) {
            send_key((UBYTE)code);
        }
    }
}

int dct13_gui_run(void)
{
    dct13_config_load(&g_cfg);
    dct13_net_init(&g_net);
    dct13_telnet_init(&g_telnet);
    g_status[0] = 0;

    if (!open_libs()) {
        close_libs();
        return 20;
    }
    if (!open_window()) {
        close_libs();
        return 20;
    }

    g_running = 1;
    while (g_running) {
        handle_window_messages();
        poll_net();
        Delay(1);
    }

    disconnect();
    dct13_net_cleanup(&g_net);
    close_window();
    close_libs();
    return 0;
}
