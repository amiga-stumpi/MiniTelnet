#include <dos/dos.h>
#include <proto/dos.h>

#include "config.h"

#define CONFIG_READ_BUF_SIZE 256
#define CONFIG_LINE_SIZE 128
#define CONFIG_WRITE_BUF_SIZE 256

static void copy_text(char *dst, UWORD max, const char *src)
{
    UWORD i;

    if (!max)
        return;
    i = 0;
    while (src && src[i] && i + 1 < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static char lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c + ('a' - 'A'));
    return c;
}

static int text_equal(const char *a, const char *b)
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

static char *skip_space(char *text)
{
    while (text && (*text == ' ' || *text == '\t'))
        ++text;
    return text;
}

static void trim_right(char *text)
{
    UWORD len;

    if (!text)
        return;
    len = 0;
    while (text[len])
        ++len;
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = 0;
        --len;
    }
}

void dct13_config_defaults(struct Dct13Config *cfg)
{
    copy_text(cfg->host, DCT13_HOST_SIZE, "telehack.com");
    copy_text(cfg->port_text, DCT13_PORT_SIZE, "23");
    copy_text(cfg->font, DCT13_FONT_NAME_SIZE, "ibm.font");
    cfg->port = 23;
    cfg->font_size = 8;
    cfg->terminal_mode = 2;
}

int dct13_parse_port(const char *text, UWORD *port_out)
{
    ULONG value;
    UWORD i;
    int seen;

    value = 0;
    i = 0;
    seen = 0;
    while (text && (text[i] == ' ' || text[i] == '\t'))
        ++i;
    while (text && text[i]) {
        if (text[i] == ' ' || text[i] == '\t') {
            while (text[i] == ' ' || text[i] == '\t')
                ++i;
            if (text[i])
                return 0;
            break;
        }
        if (text[i] < '0' || text[i] > '9')
            return 0;
        value = value * 10UL + (ULONG)(text[i] - '0');
        if (value > 65535UL)
            return 0;
        seen = 1;
        ++i;
    }
    if (!seen) {
        *port_out = 23;
        return 1;
    }
    if (value == 0)
        return 0;
    *port_out = (UWORD)value;
    return 1;
}

static int parse_uword(const char *text, UWORD *value_out, UWORD min_value, UWORD max_value)
{
    ULONG value;
    UWORD i;

    if (!text || !text[0])
        return 0;
    value = 0;
    i = 0;
    while (text[i]) {
        if (text[i] < '0' || text[i] > '9')
            return 0;
        value = value * 10UL + (ULONG)(text[i] - '0');
        if (value > max_value)
            return 0;
        ++i;
    }
    if (value < min_value)
        return 0;
    *value_out = (UWORD)value;
    return 1;
}

static void parse_config_line(struct Dct13Config *cfg, char *line)
{
    char *key;
    char *value;
    char *eq;
    UWORD parsed;

    line = skip_space(line);
    if (!line || !line[0] || line[0] == '#')
        return;
    eq = line;
    while (*eq && *eq != '=')
        ++eq;
    if (*eq != '=')
        return;
    *eq = 0;
    key = skip_space(line);
    value = skip_space(eq + 1);
    trim_right(key);
    trim_right(value);

    if (text_equal(key, "host")) {
        copy_text(cfg->host, DCT13_HOST_SIZE, value);
    } else if (text_equal(key, "port")) {
        if (dct13_parse_port(value, &parsed)) {
            cfg->port = parsed;
            copy_text(cfg->port_text, DCT13_PORT_SIZE, value);
        } else {
            cfg->port = 23;
            copy_text(cfg->port_text, DCT13_PORT_SIZE, "23");
        }
    } else if (text_equal(key, "font")) {
        copy_text(cfg->font, DCT13_FONT_NAME_SIZE, value);
    } else if (text_equal(key, "font_size")) {
        if (parse_uword(value, &parsed, 4, 32))
            cfg->font_size = parsed;
    } else if (text_equal(key, "terminal_mode")) {
        if (text_equal(value, "RAW"))
            cfg->terminal_mode = 0;
        else if (text_equal(value, "ANSI"))
            cfg->terminal_mode = 1;
        else if (text_equal(value, "ANSI_IBM"))
            cfg->terminal_mode = 2;
    }
}

void dct13_config_load(struct Dct13Config *cfg)
{
    BPTR f;
    char read_buf[CONFIG_READ_BUF_SIZE];
    char line[CONFIG_LINE_SIZE];
    LONG got;
    LONG i;
    UWORD line_len;

    dct13_config_defaults(cfg);
    f = Open((STRPTR)DCT13_CONFIG_FILE, MODE_OLDFILE);
    if (!f)
        return;

    line_len = 0;
    while ((got = Read(f, read_buf, sizeof(read_buf))) > 0) {
        for (i = 0; i < got; ++i) {
            if (read_buf[i] != '\n' && read_buf[i] != '\r') {
                if (((ULONG)line_len + 1UL) < sizeof(line))
                    line[line_len++] = read_buf[i];
                continue;
            }
            line[line_len] = 0;
            parse_config_line(cfg, line);
            line_len = 0;
        }
    }
    if (line_len > 0) {
        line[line_len] = 0;
        parse_config_line(cfg, line);
    }
    Close(f);
}

static void append_text(char *dst, ULONG dst_size, UWORD *pos, const char *src)
{
    UWORD i;

    i = 0;
    while (src && src[i] && ((ULONG)(*pos) + 1UL) < dst_size)
        dst[(*pos)++] = src[i++];
    dst[*pos] = 0;
}

static void append_uword(char *dst, ULONG dst_size, UWORD *pos, UWORD value)
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
    append_text(dst, dst_size, pos, digits + i);
}

static const char *mode_name(UBYTE mode)
{
    if (mode == 0)
        return "RAW";
    if (mode == 1)
        return "ANSI";
    return "ANSI_IBM";
}

int dct13_config_save(struct Dct13Config *cfg)
{
    BPTR f;
    char buf[CONFIG_WRITE_BUF_SIZE];
    UWORD pos;
    LONG len;

    if (!cfg)
        return 0;
    f = Open((STRPTR)DCT13_CONFIG_FILE, MODE_NEWFILE);
    if (!f)
        return 0;

    pos = 0;
    append_text(buf, sizeof(buf), &pos, "# MiniTelnet configuration\n");
    append_text(buf, sizeof(buf), &pos, "host=");
    append_text(buf, sizeof(buf), &pos, cfg->host);
    append_text(buf, sizeof(buf), &pos, "\nport=");
    if (cfg->port_text[0])
        append_text(buf, sizeof(buf), &pos, cfg->port_text);
    else
        append_uword(buf, sizeof(buf), &pos, cfg->port ? cfg->port : 23);
    append_text(buf, sizeof(buf), &pos, "\nfont=");
    append_text(buf, sizeof(buf), &pos, cfg->font);
    append_text(buf, sizeof(buf), &pos, "\nfont_size=");
    append_uword(buf, sizeof(buf), &pos, cfg->font_size ? cfg->font_size : 8);
    append_text(buf, sizeof(buf), &pos, "\nterminal_mode=");
    append_text(buf, sizeof(buf), &pos, mode_name(cfg->terminal_mode));
    append_text(buf, sizeof(buf), &pos, "\n");

    len = Write(f, buf, pos);
    Close(f);
    return len == (LONG)pos;
}
