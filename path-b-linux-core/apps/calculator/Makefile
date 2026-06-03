# vyro-calculator — Linux port on libvyro-linux + IPC

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -I../../libvyro-linux/include
LDFLAGS ?=
SRCS    := src/main.c \
           ../../libvyro-linux/src/proto.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean

all: vyro-calculator

vyro-calculator: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) vyro-calculator
