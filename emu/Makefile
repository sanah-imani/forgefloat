CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c11 -I..
LDFLAGS =

SOFTFLOAT_OBJ = ../softfloat.o

EMU_SRCS = mem.c execute.c syscall.c main.c
EMU_OBJS = $(EMU_SRCS:.c=.o)
TARGET   = riscv-emu

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(EMU_OBJS) $(SOFTFLOAT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

../softfloat.o:
	$(MAKE) -C .. softfloat.o

mem.o:     mem.c     mem.h
execute.o: execute.c execute.h decode.h cpu.h mem.h ../softfloat.h
syscall.o: syscall.c syscall.h execute.h cpu.h mem.h
main.o:    main.c    cpu.h mem.h execute.h syscall.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(EMU_OBJS) $(TARGET)
