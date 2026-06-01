#ifndef DCTELNET13_CONFIG_H
#define DCTELNET13_CONFIG_H

#include <exec/types.h>

#define DCT13_HOST_SIZE 96
#define DCT13_PORT_SIZE 8
#define DCT13_FONT_NAME_SIZE 64
#define DCT13_CONFIG_FILE "minitelnet.conf"

struct Dct13Config {
    char host[DCT13_HOST_SIZE];
    char port_text[DCT13_PORT_SIZE];
    char font[DCT13_FONT_NAME_SIZE];
    UWORD port;
    UWORD font_size;
    UBYTE terminal_mode;
};

void dct13_config_defaults(struct Dct13Config *cfg);
int dct13_parse_port(const char *text, UWORD *port_out);
void dct13_config_load(struct Dct13Config *cfg);
int dct13_config_save(struct Dct13Config *cfg);

#endif
