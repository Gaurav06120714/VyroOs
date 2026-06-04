CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -I../../libvyro-linux/include
SRCS    := src/main.c ../../libvyro-linux/src/proto.c
OBJS    := $(SRCS:.c=.o)
.PHONY: all clean
all: vyro-textedit
vyro-textedit: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) vyro-textedit
