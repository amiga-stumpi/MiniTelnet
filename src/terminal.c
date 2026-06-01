#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>

#include "terminal.h"
#include "cp437.h"

#define MIN_COLS 20
#define MIN_ROWS 5
#define DEFAULT_ATTR 0

#ifndef DCTELNET13_TERM_FONT_NAME
#define DCTELNET13_TERM_FONT_NAME "ibm.font"
#endif
#ifndef DCTELNET13_TERM_FONT_SIZE
#define DCTELNET13_TERM_FONT_SIZE 8
#endif

struct Library *DiskfontBase;

static UBYTE g_space[128];
static UBYTE g_row_text[256];


static int text_equal_nocase(const char *a, const char *b)
{
    char ca;
    char cb;

    if (!a || !b)
        return 0;
    while (*a && *b) {
        ca = *a++;
        cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb + ('a' - 'A'));
        if (ca != cb)
            return 0;
    }
    return *a == 0 && *b == 0;
}

static void update_cp437_font_mode(struct Dct13Terminal *term)
{
    if (!term)
        return;
    term->direct_cp437 = text_equal_nocase(term->font_name, "ibm.font");
}

static UWORD text_len(const char *s)
{
    UWORD n;

    n = 0;
    while (s && s[n])
        ++n;
    return n;
}

static void fill_spaces(void)
{
    UWORD i;

    for (i = 0; i < sizeof(g_space); ++i)
        g_space[i] = ' ';
}

static ULONG cell_count(UWORD cols, UWORD rows)
{
    return (ULONG)cols * (ULONG)rows;
}

static struct Dct13Cell *alloc_cells(UWORD cols, UWORD rows)
{
    ULONG bytes;
    struct Dct13Cell *cells;
    ULONG i;
    ULONG count;

    count = cell_count(cols, rows);
    bytes = count * sizeof(struct Dct13Cell);
    cells = (struct Dct13Cell *)AllocMem(bytes, MEMF_CLEAR);
    if (!cells)
        return 0;
    for (i = 0; i < count; ++i) {
        cells[i].ch = ' ';
        cells[i].attr = DEFAULT_ATTR;
    }
    return cells;
}

static void free_cells(struct Dct13Cell *cells, UWORD cols, UWORD rows)
{
    ULONG bytes;

    if (!cells)
        return;
    bytes = cell_count(cols, rows) * sizeof(struct Dct13Cell);
    FreeMem(cells, bytes);
}

static UWORD min_uword(UWORD a, UWORD b)
{
    return a < b ? a : b;
}

static UWORD calc_cols(struct Dct13Terminal *term, WORD width)
{
    UWORD cols;

    cols = (UWORD)(width / term->char_width);
    if (cols < MIN_COLS)
        cols = MIN_COLS;
    return cols;
}

static UWORD calc_rows(struct Dct13Terminal *term, WORD height)
{
    UWORD rows;

    rows = (UWORD)(height / term->char_height);
    if (rows < MIN_ROWS)
        rows = MIN_ROWS;
    return rows;
}

static void select_gui_font(struct Dct13Terminal *term)
{
    if (term && term->win && term->win->RPort && term->gui_font)
        SetFont(term->win->RPort, term->gui_font);
}

void dct13_term_restore_gui_rp(struct Dct13Terminal *term)
{
    if (!term || !term->win || !term->win->RPort)
        return;
    select_gui_font(term);
    SetAPen(term->win->RPort, 0);
    SetBPen(term->win->RPort, 1);
    SetDrMd(term->win->RPort, JAM1);
}

static void select_terminal_font(struct Dct13Terminal *term)
{
    if (term && term->win && term->win->RPort && term->terminal_font)
        SetFont(term->win->RPort, term->terminal_font);
}

static void update_font_metrics(struct Dct13Terminal *term)
{
    struct TextFont *font;

    font = term->terminal_font;
    if (font) {
        term->char_width = (WORD)font->tf_XSize;
        term->char_height = (WORD)font->tf_YSize;
        term->baseline = (WORD)font->tf_Baseline;
    }
    if (term->char_width <= 0 && term->win && term->win->RPort)
        term->char_width = term->win->RPort->TxWidth;
    if (term->char_height <= 0 && term->win && term->win->RPort)
        term->char_height = term->win->RPort->TxHeight;
    if (term->baseline <= 0)
        term->baseline = (WORD)(term->char_height - 1);
    if (term->char_width <= 0)
        term->char_width = 8;
    if (term->char_height <= 0)
        term->char_height = 8;
}

static void copy_font_name(char *dst, ULONG dst_size, const char *src)
{
    ULONG i;

    if (!dst || dst_size == 0)
        return;
    i = 0;
    while (src && src[i] && ((ULONG)i + 1UL) < dst_size) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static struct TextFont *open_named_font(const char *name, UWORD size)
{
    struct TextAttr attr;
    struct TextFont *font;

    if (!name || !name[0] || size == 0)
        return 0;
    attr.ta_Name = (STRPTR)name;
    attr.ta_YSize = size;
    attr.ta_Style = FS_NORMAL;
    attr.ta_Flags = FPF_ROMFONT | FPF_DISKFONT;

    font = OpenFont(&attr);
    if (font)
        return font;

    if (!DiskfontBase)
        DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 0);
    if (DiskfontBase)
        font = OpenDiskFont(&attr);
    return font;
}

static void install_font_or_fallback(struct Dct13Terminal *term, const char *name, UWORD size)
{
    term->terminal_font = open_named_font(name, size);
    if (term->terminal_font) {
        term->terminal_font_opened = 1;
        copy_font_name(term->font_name, sizeof(term->font_name), name);
        term->font_size = size;
        update_cp437_font_mode(term);
        return;
    }

    if (!text_equal_nocase(name, "ruby.font") || size != 8) {
        term->terminal_font = open_named_font("ruby.font", 8);
        if (term->terminal_font) {
            term->terminal_font_opened = 1;
            copy_font_name(term->font_name, sizeof(term->font_name), "ruby.font");
            term->font_size = 8;
            update_cp437_font_mode(term);
            return;
        }
    }

    if (!text_equal_nocase(name, "topaz.font") || size != 11) {
        term->terminal_font = open_named_font("topaz.font", 11);
        if (term->terminal_font) {
            term->terminal_font_opened = 1;
            copy_font_name(term->font_name, sizeof(term->font_name), "topaz.font");
            term->font_size = 11;
            update_cp437_font_mode(term);
            return;
        }
    }

    term->terminal_font = term->gui_font;
    term->terminal_font_opened = 0;
    copy_font_name(term->font_name, sizeof(term->font_name), "window font");
    term->font_size = 0;
    update_cp437_font_mode(term);
}

static void open_terminal_font(struct Dct13Terminal *term)
{
    term->gui_font = 0;
    term->terminal_font = 0;
    term->terminal_font_opened = 0;
    term->font_name[0] = 0;
    term->font_size = 0;
    if (term->win && term->win->RPort)
        term->gui_font = term->win->RPort->Font;
    install_font_or_fallback(term, DCTELNET13_TERM_FONT_NAME, DCTELNET13_TERM_FONT_SIZE);
    update_font_metrics(term);
}

static void close_terminal_font(struct Dct13Terminal *term)
{
    if (!term)
        return;
    select_gui_font(term);
    if (term->terminal_font_opened && term->terminal_font)
        CloseFont(term->terminal_font);
    term->terminal_font = 0;
    term->terminal_font_opened = 0;
    if (DiskfontBase) {
        CloseLibrary(DiskfontBase);
        DiskfontBase = 0;
    }
}

int dct13_term_apply_font(struct Dct13Terminal *term, const char *font_name, UWORD font_size)
{
    struct TextFont *new_font;
    struct TextFont *old_font;
    UBYTE old_opened;

    if (!term || !font_name || !font_name[0] || font_size == 0)
        return 0;
    new_font = open_named_font(font_name, font_size);
    if (!new_font)
        return 0;

    select_gui_font(term);
    old_font = term->terminal_font;
    old_opened = term->terminal_font_opened;
    term->terminal_font = new_font;
    term->terminal_font_opened = 1;
    copy_font_name(term->font_name, sizeof(term->font_name), font_name);
    term->font_size = font_size;
    update_cp437_font_mode(term);
    update_font_metrics(term);

    if (old_opened && old_font)
        CloseFont(old_font);
    if (term->left || term->top || term->width || term->height)
        dct13_term_resize(term, term->left, term->top, term->width, term->height);
    select_gui_font(term);
    return 1;
}

const char *dct13_term_font_name(struct Dct13Terminal *term)
{
    if (!term || !term->font_name[0])
        return "";
    return term->font_name;
}

UWORD dct13_term_font_size(struct Dct13Terminal *term)
{
    if (!term)
        return 0;
    return term->font_size;
}


static void mark_dirty_row(struct Dct13Terminal *term, UWORD row)
{
    if (!term || row >= term->rows)
        return;
    if (!term->dirty_any) {
        term->dirty_any = 1;
        term->dirty_first = row;
        term->dirty_last = row;
    } else {
        if (row < term->dirty_first)
            term->dirty_first = row;
        if (row > term->dirty_last)
            term->dirty_last = row;
    }
}

static void mark_dirty_all(struct Dct13Terminal *term)
{
    if (!term || term->rows == 0)
        return;
    term->dirty_any = 1;
    term->dirty_first = 0;
    term->dirty_last = (UWORD)(term->rows - 1);
}

static void draw_cell(struct Dct13Terminal *term, UWORD row, UWORD col)
{
    struct Dct13Cell *cell;
    WORD x;
    WORD y;
    UBYTE ch;

    if (!term->win || !term->cells || row >= term->rows || col >= term->cols)
        return;
    cell = &term->cells[(ULONG)row * term->cols + col];
    ch = cell->ch ? cell->ch : ' ';
    x = (WORD)(term->left + col * term->char_width);
    y = (WORD)(term->top + term->baseline + row * term->char_height);
    select_terminal_font(term);
    SetAPen(term->win->RPort, 1);
    SetBPen(term->win->RPort, 0);
    SetDrMd(term->win->RPort, JAM2);
    Move(term->win->RPort, x, y);
    Text(term->win->RPort, (STRPTR)&ch, 1);
    dct13_term_restore_gui_rp(term);
}

static void clear_rect(struct Dct13Terminal *term)
{
    if (!term->win)
        return;
    dct13_term_restore_gui_rp(term);
    SetAPen(term->win->RPort, 0);
    RectFill(term->win->RPort, term->left, term->top,
        (WORD)(term->left + term->width - 1),
        (WORD)(term->top + term->height - 1));
}

static void blank_cell(struct Dct13Terminal *term, UWORD row, UWORD col)
{
    struct Dct13Cell *cell;

    if (!term->cells || row >= term->rows || col >= term->cols)
        return;
    cell = &term->cells[(ULONG)row * term->cols + col];
    cell->ch = ' ';
    cell->attr = DEFAULT_ATTR;
}

static void blank_range(struct Dct13Terminal *term, UWORD row, UWORD first, UWORD last)
{
    UWORD col;

    if (row >= term->rows || first >= term->cols)
        return;
    if (last >= term->cols)
        last = (UWORD)(term->cols - 1);
    for (col = first; col <= last; ++col)
        blank_cell(term, row, col);
}

static void scroll_up(struct Dct13Terminal *term)
{
    UWORD row;
    ULONG bytes;

    if (!term->cells || term->rows == 0)
        return;
    bytes = (ULONG)term->cols * sizeof(struct Dct13Cell);
    for (row = 1; row < term->rows; ++row) {
        CopyMem(&term->cells[(ULONG)row * term->cols],
                &term->cells[(ULONG)(row - 1) * term->cols], bytes);
    }
    blank_range(term, (UWORD)(term->rows - 1), 0, (UWORD)(term->cols - 1));
    if (term->defer_draw)
        mark_dirty_all(term);
    else
        dct13_term_redraw(term);
}

static void clamp_cursor(struct Dct13Terminal *term)
{
    if (term->cols == 0 || term->rows == 0)
        return;
    if (term->cursor_col >= term->cols)
        term->cursor_col = (UWORD)(term->cols - 1);
    if (term->cursor_row >= term->rows)
        term->cursor_row = (UWORD)(term->rows - 1);
}

int dct13_term_init(struct Dct13Terminal *term, struct Window *win,
                    WORD left, WORD top, WORD width, WORD height)
{
    if (!term)
        return 0;
    term->win = win;
    term->char_width = 8;
    term->char_height = 8;
    term->baseline = 7;
    open_terminal_font(term);
    term->cells = 0;
    term->cols = 0;
    term->rows = 0;
    term->cursor_col = 0;
    term->cursor_row = 0;
    term->attr = DEFAULT_ATTR;
    term->mode = DCT13_TERM_MODE_ANSI_IBM;
    term->defer_draw = 0;
    term->dirty_any = 0;
    term->dirty_first = 0;
    term->dirty_last = 0;
    dct13_ansi_init(&term->ansi);
    fill_spaces();
    return dct13_term_resize(term, left, top, width, height);
}

void dct13_term_free(struct Dct13Terminal *term)
{
    if (!term)
        return;
    free_cells(term->cells, term->cols, term->rows);
    close_terminal_font(term);
    term->cells = 0;
    term->cols = 0;
    term->rows = 0;
}

int dct13_term_resize(struct Dct13Terminal *term,
                      WORD left, WORD top, WORD width, WORD height)
{
    UWORD new_cols;
    UWORD new_rows;
    struct Dct13Cell *new_cells;
    UWORD copy_cols;
    UWORD copy_rows;
    UWORD row;
    ULONG old_off;
    ULONG new_off;
    ULONG bytes;

    if (!term || width <= 0 || height <= 0)
        return 0;
    new_cols = calc_cols(term, width);
    new_rows = calc_rows(term, height);
    new_cells = alloc_cells(new_cols, new_rows);
    if (!new_cells)
        return 0;
    if (term->cells) {
        copy_cols = min_uword(term->cols, new_cols);
        copy_rows = min_uword(term->rows, new_rows);
        bytes = (ULONG)copy_cols * sizeof(struct Dct13Cell);
        for (row = 0; row < copy_rows; ++row) {
            old_off = (ULONG)row * term->cols;
            new_off = (ULONG)row * new_cols;
            CopyMem(&term->cells[old_off], &new_cells[new_off], bytes);
        }
        free_cells(term->cells, term->cols, term->rows);
    }
    term->left = left;
    term->top = top;
    term->width = width;
    term->height = height;
    term->cells = new_cells;
    term->cols = new_cols;
    term->rows = new_rows;
    term->status_top = (WORD)(top + height + 4);
    clamp_cursor(term);
    dct13_term_redraw(term);
    return 1;
}

void dct13_term_clear(struct Dct13Terminal *term)
{
    ULONG i;
    ULONG count;

    if (!term || !term->cells)
        return;
    count = cell_count(term->cols, term->rows);
    for (i = 0; i < count; ++i) {
        term->cells[i].ch = ' ';
        term->cells[i].attr = DEFAULT_ATTR;
    }
    term->cursor_col = 0;
    term->cursor_row = 0;
    dct13_ansi_init(&term->ansi);
    if (term->defer_draw)
        mark_dirty_all(term);
    else
        dct13_term_redraw(term);
}

static void draw_row(struct Dct13Terminal *term, UWORD row)
{
    UWORD col;
    UWORD n;
    WORD y;

    if (!term || !term->win || !term->cells || row >= term->rows)
        return;
    n = term->cols;
    if (n > sizeof(g_row_text))
        n = sizeof(g_row_text);
    for (col = 0; col < n; ++col) {
        g_row_text[col] = term->cells[(ULONG)row * term->cols + col].ch;
        if (!g_row_text[col])
            g_row_text[col] = ' ';
    }
    select_terminal_font(term);
    SetAPen(term->win->RPort, 0);
    RectFill(term->win->RPort, term->left, (WORD)(term->top + row * term->char_height),
        (WORD)(term->left + term->width - 1),
        (WORD)(term->top + (row + 1) * term->char_height - 1));
    SetAPen(term->win->RPort, 1);
    SetBPen(term->win->RPort, 0);
    SetDrMd(term->win->RPort, JAM2);
    y = (WORD)(term->top + term->baseline + row * term->char_height);
    Move(term->win->RPort, term->left, y);
    Text(term->win->RPort, (STRPTR)g_row_text, n);
    dct13_term_restore_gui_rp(term);
}

static void flush_dirty_rows(struct Dct13Terminal *term)
{
    UWORD row;

    if (!term || !term->dirty_any)
        return;
    for (row = term->dirty_first; row <= term->dirty_last && row < term->rows; ++row)
        draw_row(term, row);
    term->dirty_any = 0;
}

void dct13_term_redraw(struct Dct13Terminal *term)
{
    UWORD row;

    if (!term || !term->win)
        return;
    clear_rect(term);
    for (row = 0; row < term->rows; ++row)
        draw_row(term, row);
    term->dirty_any = 0;
}

void dct13_term_put_char(struct Dct13Terminal *term, UBYTE ch)
{
    struct Dct13Cell *cell;

    if (!term || !term->cells || term->cols == 0 || term->rows == 0)
        return;
    if (term->mode == DCT13_TERM_MODE_ANSI_IBM && !term->direct_cp437)
        ch = dct13_cp437_to_amiga(ch);
    clamp_cursor(term);
    cell = &term->cells[(ULONG)term->cursor_row * term->cols + term->cursor_col];
    cell->ch = ch;
    cell->attr = term->attr;
    if (term->defer_draw)
        mark_dirty_row(term, term->cursor_row);
    else
        draw_cell(term, term->cursor_row, term->cursor_col);
    ++term->cursor_col;
    if (term->cursor_col >= term->cols) {
        term->cursor_col = 0;
        dct13_term_lf(term);
    }
}

void dct13_term_cr(struct Dct13Terminal *term)
{
    if (term)
        term->cursor_col = 0;
}

void dct13_term_lf(struct Dct13Terminal *term)
{
    if (!term || term->rows == 0)
        return;
    if ((ULONG)term->cursor_row + 1UL >= term->rows) {
        scroll_up(term);
    } else {
        ++term->cursor_row;
    }
}

void dct13_term_bs(struct Dct13Terminal *term)
{
    if (!term)
        return;
    if (term->cursor_col > 0)
        --term->cursor_col;
}

void dct13_term_tab(struct Dct13Terminal *term)
{
    UWORD next;

    if (!term || term->cols == 0)
        return;
    next = (UWORD)((term->cursor_col + 8U) & ~7U);
    while (term->cursor_col < next)
        dct13_term_put_char(term, ' ');
}

void dct13_term_move_cursor(struct Dct13Terminal *term, WORD row, WORD col)
{
    if (!term || term->rows == 0 || term->cols == 0)
        return;
    if (row < 0)
        row = 0;
    if (col < 0)
        col = 0;
    if ((UWORD)row >= term->rows)
        row = (WORD)(term->rows - 1);
    if ((UWORD)col >= term->cols)
        col = (WORD)(term->cols - 1);
    term->cursor_row = (UWORD)row;
    term->cursor_col = (UWORD)col;
}

void dct13_term_move_relative(struct Dct13Terminal *term, WORD drow, WORD dcol)
{
    WORD row;
    WORD col;

    if (!term)
        return;
    row = (WORD)term->cursor_row;
    col = (WORD)term->cursor_col;
    row = (WORD)(row + drow);
    col = (WORD)(col + dcol);
    dct13_term_move_cursor(term, row, col);
}

void dct13_term_clear_screen(struct Dct13Terminal *term, UWORD mode)
{
    UWORD row;

    if (!term || !term->cells)
        return;
    if (mode == 2) {
        dct13_term_clear(term);
        return;
    }
    if (mode == 1) {
        for (row = 0; row < term->cursor_row; ++row)
            blank_range(term, row, 0, (UWORD)(term->cols - 1));
        blank_range(term, term->cursor_row, 0, term->cursor_col);
    } else {
        blank_range(term, term->cursor_row, term->cursor_col, (UWORD)(term->cols - 1));
        for (row = (UWORD)(term->cursor_row + 1); row < term->rows; ++row)
            blank_range(term, row, 0, (UWORD)(term->cols - 1));
    }
    if (term->defer_draw)
        mark_dirty_all(term);
    else
        dct13_term_redraw(term);
}

void dct13_term_clear_line(struct Dct13Terminal *term, UWORD mode)
{
    if (!term || !term->cells)
        return;
    if (mode == 2)
        blank_range(term, term->cursor_row, 0, (UWORD)(term->cols - 1));
    else if (mode == 1)
        blank_range(term, term->cursor_row, 0, term->cursor_col);
    else
        blank_range(term, term->cursor_row, term->cursor_col, (UWORD)(term->cols - 1));
    if (term->defer_draw)
        mark_dirty_row(term, term->cursor_row);
    else
        draw_row(term, term->cursor_row);
}

void dct13_term_set_attr(struct Dct13Terminal *term, UWORD code)
{
    if (!term)
        return;
    if (code == 0) {
        term->attr = DEFAULT_ATTR;
    } else if (code == 1) {
        term->attr |= 1;
    } else if (code == 7) {
        term->attr |= 2;
    } else if ((code >= 30 && code <= 37) || (code >= 40 && code <= 47)) {
        /* Parsed and stored later; drawing remains monochrome in the internal renderer. */
        term->attr = (UBYTE)(term->attr | 4);
    }
}

void dct13_term_set_mode(struct Dct13Terminal *term, UBYTE mode)
{
    if (!term)
        return;
    if (mode > DCT13_TERM_MODE_ANSI_IBM)
        mode = DCT13_TERM_MODE_ANSI_IBM;
    term->mode = mode;
    dct13_ansi_init(&term->ansi);
}

static void term_write_raw(struct Dct13Terminal *term, const UBYTE *data, UWORD len)
{
    UWORD i;
    UBYTE ch;

    for (i = 0; i < len; ++i) {
        ch = data[i];
        if (ch == '\r')
            dct13_term_cr(term);
        else if (ch == '\n')
            dct13_term_lf(term);
        else if (ch == 8 || ch == 127)
            dct13_term_bs(term);
        else if (ch == '\t')
            dct13_term_tab(term);
        else if (ch >= 32)
            dct13_term_put_char(term, ch);
    }
}

void dct13_term_write(struct Dct13Terminal *term, const UBYTE *data, UWORD len)
{
    UBYTE old_defer;

    if (!term || !term->win || !data)
        return;
    old_defer = term->defer_draw;
    term->defer_draw = 1;
    if (term->mode == DCT13_TERM_MODE_RAW)
        term_write_raw(term, data, len);
    else
        dct13_ansi_feed(&term->ansi, term, data, len);
    term->defer_draw = old_defer;
    if (!term->defer_draw)
        flush_dirty_rows(term);
}

void dct13_term_status(struct Dct13Terminal *term, const char *text)
{
    WORD y;
    UWORD len;

    if (!term || !term->win)
        return;
    dct13_term_restore_gui_rp(term);
    y = (WORD)(term->win->Height - 8);
    SetAPen(term->win->RPort, 1);
    RectFill(term->win->RPort, 4, (WORD)(y - 7), (WORD)(term->win->Width - 5), y);
    select_gui_font(term);
    SetAPen(term->win->RPort, 0);
    SetBPen(term->win->RPort, 1);
    SetDrMd(term->win->RPort, JAM2);
    Move(term->win->RPort, 4, y);
    len = text_len(text);
    if (len > 78)
        len = 78;
    Text(term->win->RPort, (STRPTR)text, len);
    dct13_term_restore_gui_rp(term);
}
