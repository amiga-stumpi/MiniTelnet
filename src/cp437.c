#include <exec/types.h>

#include "cp437.h"

#if DCTELNET13_TERM_DEBUG
#include <proto/dos.h>
static void cp437_debug_unknown(UBYTE c)
{
    static const char hex[] = "0123456789abcdef";
    char msg[32];

    msg[0] = 'D'; msg[1] = 'C'; msg[2] = 'T'; msg[3] = ' ';
    msg[4] = 'C'; msg[5] = 'P'; msg[6] = '4'; msg[7] = '3'; msg[8] = '7';
    msg[9] = ' '; msg[10] = '0'; msg[11] = 'x';
    msg[12] = hex[(c >> 4) & 0x0f];
    msg[13] = hex[c & 0x0f];
    msg[14] = '\n'; msg[15] = 0;
    Write(Output(), (APTR)msg, 15);
}
#else
#define cp437_debug_unknown(c) ((void)0)
#endif

UBYTE dct13_cp437_to_amiga(UBYTE c)
{
    if (c >= 0x20 && c <= 0x7e)
        return c;

    switch (c) {
    case 0x01: return '*';
    case 0x02: return '*';
    case 0x03: return '*';
    case 0x04: return '*';
    case 0x05: return '*';
    case 0x06: return '*';
    case 0x07: return '*';
    case 0x08: return '*';
    case 0x09: return 'o';
    case 0x0a: return 'o';
    case 0x0b: return '*';
    case 0x0c: return '*';
    case 0x10: return '>';
    case 0x11: return '<';
    case 0x12: return '!';
    case 0x13: return '!';
    case 0x18: return '^';
    case 0x19: return 'v';
    case 0x1a: return '>';
    case 0x1b: return '<';
    case 0x1e: return '^';
    case 0x1f: return 'v';

    case 0xb0: return '.';
    case 0xb1: return ':';
    case 0xb2: return '#';
    case 0xdb: return '#';
    case 0xdc: return '#';
    case 0xdd: return '#';
    case 0xde: return '#';
    case 0xdf: return '#';

    case 0xb3: return '|';
    case 0xba: return '|';
    case 0xb4: return '+';
    case 0xb5: return '+';
    case 0xb6: return '+';
    case 0xb7: return '+';
    case 0xb8: return '+';
    case 0xb9: return '+';
    case 0xbb: return '+';
    case 0xbc: return '+';
    case 0xbd: return '+';
    case 0xbe: return '+';
    case 0xbf: return '+';
    case 0xc0: return '+';
    case 0xc1: return '+';
    case 0xc2: return '+';
    case 0xc3: return '+';
    case 0xc4: return '-';
    case 0xc5: return '+';
    case 0xc6: return '+';
    case 0xc7: return '+';
    case 0xc8: return '+';
    case 0xc9: return '+';
    case 0xca: return '+';
    case 0xcb: return '+';
    case 0xcc: return '+';
    case 0xcd: return '=';
    case 0xce: return '+';
    case 0xcf: return '+';
    case 0xd0: return '+';
    case 0xd1: return '+';
    case 0xd2: return '+';
    case 0xd3: return '+';
    case 0xd4: return '+';
    case 0xd5: return '+';
    case 0xd6: return '+';
    case 0xd7: return '+';
    case 0xd8: return '+';
    case 0xd9: return '+';
    case 0xda: return '+';

    case 0xf8: return 'o';
    case 0xf9: return '.';
    case 0xfa: return '.';
    case 0xfb: return 'v';
    case 0xfc: return 'n';
    case 0xfd: return '2';
    case 0xfe: return '#';

    case 0xf1: return '+';
    case 0xf6: return '/';
    case 0xf7: return '~';

    default:
        cp437_debug_unknown(c);
        if (c >= 0x80 && c <= 0xaf)
            return '?';
        if (c >= 0xe0 && c <= 0xef)
            return '?';
        return '?';
    }
}
