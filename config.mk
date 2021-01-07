PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man
MANDIR = $(MANPREFIX)/man1

CFLAGS += -std=c99 -Wall -g -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=500
LDFLAGS += -lxcb -lxcb-ewmh -lxcb-icccm -lwm -lxcb-randr -lxcb-cursor
