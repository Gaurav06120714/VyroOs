# Path A wrapper for the Path B calculator app.
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -D_GNU_SOURCE \
           -I../../../path-b-linux-core/libvyro-linux/include
SRCDIR  := ../../../path-b-linux-core
SRCS    := $(SRCDIR)/apps/calculator/src/main.c $(SRCDIR)/libvyro-linux/src/proto.c

PREFIX  ?= /usr
BINDIR  ?= $(PREFIX)/bin

.PHONY: all install clean
all: vyro-calculator

vyro-calculator: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

install: vyro-calculator
	install -D -m 0755 vyro-calculator $(DESTDIR)$(BINDIR)/vyro-calculator

clean:
	rm -f vyro-calculator
