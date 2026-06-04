CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -I../../libvyro-linux/include
LDLIBS  ?= -lutil    # forkpty
SRCS    := src/main.c ../../libvyro-linux/src/proto.c
OBJS    := $(SRCS:.c=.o)
.PHONY: all clean
all: vyro-terminal
vyro-terminal: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) vyro-terminal
