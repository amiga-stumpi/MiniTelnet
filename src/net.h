#ifndef DCTELNET13_NET_H
#define DCTELNET13_NET_H

#include <exec/types.h>

#define DCT13_NET_OK 0
#define DCT13_NET_ERROR -1
#define DCT13_NET_WOULDBLOCK -2
#define DCT13_NET_CLOSED -3

struct Dct13Net {
    struct Library *base;
    int fd;
    int last_errno;
    ULONG last_ip;
};

void dct13_net_init(struct Dct13Net *net);
void dct13_net_cleanup(struct Dct13Net *net);
int dct13_net_open(struct Dct13Net *net);
int dct13_net_connect(struct Dct13Net *net, const char *host, UWORD port, LONG timeout_sec);
int dct13_net_send(struct Dct13Net *net, const UBYTE *buf, UWORD len);
int dct13_net_recv(struct Dct13Net *net, UBYTE *buf, UWORD len);
void dct13_net_close(struct Dct13Net *net);
int dct13_net_connected(const struct Dct13Net *net);

#endif
