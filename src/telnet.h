#ifndef DCTELNET13_TELNET_H
#define DCTELNET13_TELNET_H

#include <exec/types.h>

struct Dct13Telnet {
    UBYTE state;
    UBYTE command;
};

void dct13_telnet_init(struct Dct13Telnet *tn);
UWORD dct13_telnet_filter(struct Dct13Telnet *tn,
                          const UBYTE *in,
                          UWORD in_len,
                          UBYTE *out,
                          UWORD out_max,
                          UBYTE *reply,
                          UWORD reply_max);

#endif
