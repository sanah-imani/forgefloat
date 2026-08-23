CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -I..
LDFLAGS =

SOFTFLOAT_OBJ = ../softfloat.o

EMU_SRCS = mem.c
EMU_OBJS = $(EMU_SRCS:.c=.o)

.PHONY: all clean

all: $(EMU_OBJS)

../softfloat.o:
	$(MAKE) -C .. softfloat.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

mem.o: mem.c mem.h

clean:
	rm -f $(EMU_OBJS)
