# vyro-files — Linux port

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -I../../libvyro-linux/include
LDFLAGS ?=
SRCS    := src/main.c ../../libvyro-linux/src/proto.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean
all: vyro-files
vyro-files: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) vyro-files
