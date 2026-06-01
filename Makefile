PROJECT := MiniTelnet

AMIGA_PREFIX ?= /opt/amiga
NETINCLUDE_DIR ?= /opt/amiga-netinclude

CROSS := $(AMIGA_PREFIX)/bin/m68k-amigaos-
CC := $(CROSS)gcc

CPPFLAGS := -Iinclude -I$(NETINCLUDE_DIR)/include
CFLAGS := -O2 -Wall -Wextra -mcrt=nix13 -DAMITCP13_OS13

DCTELNET13_ANSI_DEBUG ?= 0
DCTELNET13_TERM_DEBUG ?= 0
DCTELNET13_RENDER_DEBUG ?= 0
DCTELNET13_NET_DEBUG ?= 0
DCTELNET13_TERM_FONT_NAME ?= ruby.font
DCTELNET13_TERM_FONT_SIZE ?= 8

CFLAGS += -DDCTELNET13_ANSI_DEBUG=$(DCTELNET13_ANSI_DEBUG)
CFLAGS += -DDCTELNET13_TERM_DEBUG=$(DCTELNET13_TERM_DEBUG)
CFLAGS += -DDCTELNET13_RENDER_DEBUG=$(DCTELNET13_RENDER_DEBUG)
CFLAGS += -DDCTELNET13_NET_DEBUG=$(DCTELNET13_NET_DEBUG)
CFLAGS += -DDCTELNET13_TERM_FONT_NAME=\"$(DCTELNET13_TERM_FONT_NAME)\"
CFLAGS += -DDCTELNET13_TERM_FONT_SIZE=$(DCTELNET13_TERM_FONT_SIZE)

SRCS = \
	src/main.c \
	src/net.c \
	src/telnet.c \
	src/ansi.c \
	src/cp437.c \
	src/terminal.c \
	src/gui_os13.c \
	src/xfer_xpr.c \
	src/xfer_xem.c \
	src/config.c

OBJS = $(SRCS:.c=.o)

all: build/MiniTelnet

build:
	mkdir -p build

build/MiniTelnet: build $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) build/MiniTelnet

.PHONY: all clean
