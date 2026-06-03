# Path A wrapper for the Path B files app.
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -D_GNU_SOURCE \
           -I../../../path-b-linux-core/libvyro-linux/include
SRCDIR  := ../../../path-b-linux-core
SRCS    := $(SRCDIR)/apps/files/src/main.c $(SRCDIR)/libvyro-linux/src/proto.c

PREFIX  ?= /usr
BINDIR  ?= $(PREFIX)/bin

.PHONY: all install clean
all: vyro-files

vyro-files: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

install: vyro-files
	install -D -m 0755 vyro-files $(DESTDIR)$(BINDIR)/vyro-files

clean:
	rm -f vyro-files
