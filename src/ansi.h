#ifndef DCTELNET13_ANSI_H
#define DCTELNET13_ANSI_H

#include <exec/types.h>

struct Dct13Terminal;

#define DCT13_ANSI_PARAMS 8

struct Dct13Ansi {
    UBYTE state;
    UBYTE private_mode;
    UBYTE param_count;
    UWORD params[DCT13_ANSI_PARAMS];
};

void dct13_ansi_init(struct Dct13Ansi *ansi);
void dct13_ansi_feed(struct Dct13Ansi *ansi, struct Dct13Terminal *term,
                     const UBYTE *data, UWORD len);

#endif
