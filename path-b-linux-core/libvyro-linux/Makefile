# libvyro-linux — static library build
#
# Invoked by Buildroot (CC, AR overridden). Produces libvyro.a.

CC      ?= cc
AR      ?= ar
CFLAGS  ?= -O2 -Wall -Wextra -fPIC -Iinclude
SRCS    := src/window.c src/draw.c src/input.c src/lifecycle.c src/proto.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean

all: libvyro.a

libvyro.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) libvyro.a
