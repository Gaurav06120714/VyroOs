# Thin Path A build wrapper for the Path B compositor source tree.
# Sources live at ../../../path-b-linux-core/compositor-drm/src/ so a single
# canonical compositor implementation serves both paths.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -D_GNU_SOURCE \
           -I../../../path-b-linux-core/libvyro-linux/include
SRCDIR  := ../../../path-b-linux-core/compositor-drm/src
SRCS    := $(SRCDIR)/main.c $(SRCDIR)/server.c $(SRCDIR)/chrome.c $(SRCDIR)/input.c

PKG_CFLAGS := $(shell pkg-config --cflags libdrm libinput)
PKG_LIBS   := $(shell pkg-config --libs   libdrm libinput)

PREFIX  ?= /usr
BINDIR  ?= $(PREFIX)/bin

.PHONY: all install clean
all: vyro-compositor

vyro-compositor: $(SRCS)
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -o $@ $(SRCS) $(PKG_LIBS)

install: vyro-compositor
	install -D -m 0755 vyro-compositor $(DESTDIR)$(BINDIR)/vyro-compositor

clean:
	rm -f vyro-compositor
