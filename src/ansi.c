#include <exec/types.h>

#include "ansi.h"
#include "terminal.h"

#define ANSI_NORMAL 0
#define ANSI_ESC 1
#define ANSI_CSI 2
#define ANSI_CHARSET 3

#if DCTELNET13_ANSI_DEBUG || DCTELNET13_TERM_DEBUG
#include <proto/dos.h>
static void ansi_debug_s(const char *s)
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
#else
#define ansi_debug_s(s) ((void)0)
#endif

void dct13_ansi_init(struct Dct13Ansi *ansi)
{
    UWORD i;

    if (!ansi)
        return;
    ansi->state = ANSI_NORMAL;
    ansi->private_mode = 0;
    ansi->param_count = 0;
    for (i = 0; i < DCT13_ANSI_PARAMS; ++i)
        ansi->params[i] = 0;
}

static UWORD ansi_param(struct Dct13Ansi *ansi, UWORD index, UWORD def)
{
    if (!ansi || index >= ansi->param_count)
        return def;
    if (ansi->params[index] == 0)
        return def;
    return ansi->params[index];
}

static void ansi_csi_dispatch(struct Dct13Ansi *ansi, struct Dct13Terminal *term, UBYTE final)
{
    UWORD n;
    UWORD row;
    UWORD col;
    UWORD i;

    switch (final) {
    case 'A':
        n = ansi_param(ansi, 0, 1);
        dct13_term_move_relative(term, (WORD)(0 - (WORD)n), 0);
        break;
    case 'B':
        n = ansi_param(ansi, 0, 1);
        dct13_term_move_relative(term, (WORD)n, 0);
        break;
    case 'C':
        n = ansi_param(ansi, 0, 1);
        dct13_term_move_relative(term, 0, (WORD)n);
        break;
    case 'D':
        n = ansi_param(ansi, 0, 1);
        dct13_term_move_relative(term, 0, (WORD)(0 - (WORD)n));
        break;
    case 'E':
        n = ansi_param(ansi, 0, 1);
        dct13_term_move_relative(term, (WORD)n, 0);
        dct13_term_cr(term);
        break;
    case 'F':
        n = ansi_param(ansi, 0, 1);
        dct13_term_move_relative(term, (WORD)(0 - (WORD)n), 0);
        dct13_term_cr(term);
        break;
    case 'G':
        col = ansi_param(ansi, 0, 1);
        dct13_term_move_cursor(term, (WORD)term->cursor_row, (WORD)(col - 1));
        break;
    case 'd':
        row = ansi_param(ansi, 0, 1);
        dct13_term_move_cursor(term, (WORD)(row - 1), (WORD)term->cursor_col);
        break;
    case 'H':
    case 'f':
        row = ansi_param(ansi, 0, 1);
        col = ansi_param(ansi, 1, 1);
        dct13_term_move_cursor(term, (WORD)(row - 1), (WORD)(col - 1));
        break;
    case 'J':
        dct13_term_clear_screen(term, ansi_param(ansi, 0, 0));
        break;
    case 'K':
        dct13_term_clear_line(term, ansi_param(ansi, 0, 0));
        break;
    case 'h':
    case 'l':
        if (ansi->private_mode && ansi_param(ansi, 0, 0) == 7)
            dct13_term_set_auto_wrap(term, final == 'h');
        break;
    case 's':
        dct13_term_save_cursor(term);
        break;
    case 'u':
        dct13_term_restore_cursor(term);
        break;
    case 'm':
        if (ansi->param_count == 0) {
            dct13_term_set_attr(term, 0);
        } else {
            for (i = 0; i < ansi->param_count; ++i)
                dct13_term_set_attr(term, ansi->params[i]);
        }
        break;
    default:
        ansi_debug_s("MiniTelnet ANSI unknown CSI\n");
        break;
    }
}

static void ansi_csi_char(struct Dct13Ansi *ansi, struct Dct13Terminal *term, UBYTE ch)
{
    UBYTE idx;

    if (ch >= '0' && ch <= '9') {
        idx = ansi->param_count;
        if (idx >= DCT13_ANSI_PARAMS)
            idx = DCT13_ANSI_PARAMS - 1;
        ansi->params[idx] = (UWORD)(ansi->params[idx] * 10U + (UWORD)(ch - '0'));
        if (ansi->param_count == 0)
            ansi->param_count = 1;
        return;
    }
    if ((ch >= 0x20 && ch <= 0x2f) || ch == '=') {
        return;
    }
    if (ch == ';') {
        if (ansi->param_count < DCT13_ANSI_PARAMS)
            ++ansi->param_count;
        return;
    }
    if (ch == '?') {
        ansi->private_mode = 1;
        return;
    }
    ansi_csi_dispatch(ansi, term, ch);
    dct13_ansi_init(ansi);
}

void dct13_ansi_feed(struct Dct13Ansi *ansi, struct Dct13Terminal *term,
                     const UBYTE *data, UWORD len)
{
    UWORD i;
    UBYTE ch;

    if (!ansi || !term || !data)
        return;
    for (i = 0; i < len; ++i) {
        ch = data[i];
        if (ansi->state == ANSI_ESC) {
            if (ch == '[') {
                ansi->state = ANSI_CSI;
                ansi->private_mode = 0;
                ansi->param_count = 0;
                continue;
            }
            if (ch == '(' || ch == ')') {
                ansi->state = ANSI_CHARSET;
                continue;
            }
            if (ch == 'c') {
                dct13_term_clear(term);
            } else if (ch == 'D') {
                dct13_term_lf(term);
            } else if (ch == 'M') {
                /* Reverse index is ignored in v0.2. */
            } else if (ch == '7') {
                dct13_term_save_cursor(term);
            } else if (ch == '8') {
                dct13_term_restore_cursor(term);
            } else {
                ansi_debug_s("MiniTelnet ANSI unknown ESC\n");
            }
            dct13_ansi_init(ansi);
            continue;
        }
        if (ansi->state == ANSI_CSI) {
            ansi_csi_char(ansi, term, ch);
            continue;
        }
        if (ansi->state == ANSI_CHARSET) {
            dct13_ansi_init(ansi);
            continue;
        }
        if (ch == 27) {
            ansi->state = ANSI_ESC;
        } else if (ch == '\r') {
            dct13_term_cr(term);
        } else if (ch == '\n') {
            dct13_term_lf(term);
        } else if (ch == 8 || ch == 127) {
            dct13_term_bs(term);
        } else if (ch == '\t') {
            dct13_term_tab(term);
        } else if (ch == 7) {
            /* BEL ignored for v0.2. */
        } else if (ch >= 32) {
            dct13_term_put_char(term, ch);
        }
    }
}
