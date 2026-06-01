#ifndef DCTELNET13_XFER_XPR_H
#define DCTELNET13_XFER_XPR_H

#include <exec/types.h>
#include "net.h"

#define DCT13_XFER_OK 0
#define DCT13_XFER_ERROR -1
#define DCT13_XFER_NO_LIBRARY -2
#define DCT13_XFER_NOT_CONNECTED -3

typedef void (*Dct13XferStatusFunc)(const char *text, void *user);

int dct13_xpr_available(void);
int dct13_xpr_receive_zmodem(struct Dct13Net *net,
                             Dct13XferStatusFunc status_func,
                             void *status_user);

#endif
