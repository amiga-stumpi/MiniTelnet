#ifndef DCTELNET13_TERMINAL_H
#define DCTELNET13_TERMINAL_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/text.h>

#include "ansi.h"

#define DCT13_TERM_MODE_RAW 0
#define DCT13_TERM_MODE_ANSI 1
#define DCT13_TERM_MODE_ANSI_IBM 2

struct Dct13Cell {
    UBYTE ch;
    UBYTE attr;
};

struct Dct13Terminal {
    struct Window *win;
    WORD left;
    WORD top;
    WORD width;
    WORD height;
    WORD status_top;
    WORD char_width;
    WORD char_height;
    WORD baseline;
    struct TextFont *gui_font;
    struct TextFont *terminal_font;
    UBYTE terminal_font_opened;
    char font_name[64];
    UWORD font_size;
    UWORD cols;
    UWORD rows;
    UWORD cursor_col;
    UWORD cursor_row;
    UBYTE attr;
    UBYTE mode;
    UBYTE defer_draw;
    UBYTE dirty_any;
    UWORD dirty_first;
    UWORD dirty_last;
    struct Dct13Cell *cells;
    struct Dct13Ansi ansi;
};

int dct13_term_init(struct Dct13Terminal *term, struct Window *win,
                    WORD left, WORD top, WORD width, WORD height);
void dct13_term_free(struct Dct13Terminal *term);
int dct13_term_resize(struct Dct13Terminal *term,
                      WORD left, WORD top, WORD width, WORD height);
void dct13_term_clear(struct Dct13Terminal *term);
void dct13_term_redraw(struct Dct13Terminal *term);
void dct13_term_set_mode(struct Dct13Terminal *term, UBYTE mode);
void dct13_term_write(struct Dct13Terminal *term, const UBYTE *data, UWORD len);
void dct13_term_status(struct Dct13Terminal *term, const char *text);
int dct13_term_apply_font(struct Dct13Terminal *term, const char *font_name,
                          UWORD font_size);
const char *dct13_term_font_name(struct Dct13Terminal *term);
UWORD dct13_term_font_size(struct Dct13Terminal *term);
void dct13_term_restore_gui_rp(struct Dct13Terminal *term);

void dct13_term_put_char(struct Dct13Terminal *term, UBYTE ch);
void dct13_term_cr(struct Dct13Terminal *term);
void dct13_term_lf(struct Dct13Terminal *term);
void dct13_term_bs(struct Dct13Terminal *term);
void dct13_term_tab(struct Dct13Terminal *term);
void dct13_term_move_cursor(struct Dct13Terminal *term, WORD row, WORD col);
void dct13_term_move_relative(struct Dct13Terminal *term, WORD drow, WORD dcol);
void dct13_term_clear_screen(struct Dct13Terminal *term, UWORD mode);
void dct13_term_clear_line(struct Dct13Terminal *term, UWORD mode);
void dct13_term_set_attr(struct Dct13Terminal *term, UWORD code);

#endif
