#include "telnet.h"

#define IAC  255
#define DONT 254
#define DO   253
#define WONT 252
#define WILL 251
#define SB   250
#define SE   240

#define ST_DATA   0
#define ST_IAC    1
#define ST_OPT    2
#define ST_SB     3
#define ST_SB_IAC 4

void dct13_telnet_init(struct Dct13Telnet *tn)
{
    tn->state = ST_DATA;
    tn->command = 0;
}

static void put_reply(UBYTE *reply, UWORD *len, UWORD max, UBYTE cmd, UBYTE opt)
{
    if (*len + 3 > max)
        return;
    reply[*len] = IAC;
    reply[*len + 1] = cmd;
    reply[*len + 2] = opt;
    *len = (UWORD)(*len + 3);
}

UWORD dct13_telnet_filter(struct Dct13Telnet *tn,
                          const UBYTE *in,
                          UWORD in_len,
                          UBYTE *out,
                          UWORD out_max,
                          UBYTE *reply,
                          UWORD reply_max)
{
    UWORD i;
    UWORD out_len;
    UWORD reply_len;
    UBYTE ch;

    out_len = 0;
    reply_len = 0;
    for (i = 0; i < in_len; ++i) {
        ch = in[i];
        if (tn->state == ST_DATA) {
            if (ch == IAC)
                tn->state = ST_IAC;
            else if (out_len < out_max)
                out[out_len++] = ch;
        } else if (tn->state == ST_IAC) {
            if (ch == IAC) {
                if (out_len < out_max)
                    out[out_len++] = ch;
                tn->state = ST_DATA;
            } else if (ch == DO || ch == DONT || ch == WILL || ch == WONT) {
                tn->command = ch;
                tn->state = ST_OPT;
            } else if (ch == SB) {
                tn->state = ST_SB;
            } else {
                tn->state = ST_DATA;
            }
        } else if (tn->state == ST_OPT) {
            if (tn->command == DO)
                put_reply(reply, &reply_len, reply_max, WONT, ch);
            else if (tn->command == WILL)
                put_reply(reply, &reply_len, reply_max, DONT, ch);
            tn->state = ST_DATA;
        } else if (tn->state == ST_SB) {
            if (ch == IAC)
                tn->state = ST_SB_IAC;
        } else if (tn->state == ST_SB_IAC) {
            if (ch == SE)
                tn->state = ST_DATA;
            else if (ch != IAC)
                tn->state = ST_SB;
        }
    }
    return (UWORD)((reply_len << 8) | (out_len & 0xff));
}
