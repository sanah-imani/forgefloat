# softfloat Makefile

CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS = -lm

# Target: library object + test binary
LIB_OBJ = softfloat.o
TEST_BIN = softfloat_test

.PHONY: all test clean

all: $(TEST_BIN)

$(LIB_OBJ): softfloat.c softfloat.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_BIN): softfloat_test.c $(LIB_OBJ) softfloat.h
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJ) $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

# Run with a different random seed
test-seed: $(TEST_BIN)
	./$(TEST_BIN) 1337

clean:
	rm -f $(LIB_OBJ) $(TEST_BIN)
