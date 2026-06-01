#ifndef DCTELNET13_XFER_XEM_H
#define DCTELNET13_XFER_XEM_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include "net.h"

#define DCT13_XEM_OK 0
#define DCT13_XEM_ERROR -1
#define DCT13_XEM_NO_LIBRARY -2
#define DCT13_XEM_NO_KEYMAP -3
#define DCT13_XEM_NO_DISKFONT -4
#define DCT13_XEM_SETUP_FAILED -5
#define DCT13_XEM_OPEN_CONSOLE_FAILED -6

int dct13_xem_open(struct Window *win, struct TextFont *font, struct Dct13Net *net,
                   const char *library_name);
void dct13_xem_close(void);
int dct13_xem_active(void);
void dct13_xem_write(const UBYTE *data, UWORD len);
void dct13_xem_clear(void);

#endif
