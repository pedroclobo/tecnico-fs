CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L

SOURCES := $(wildcard */*.c)
HEADERS := $(wildcard */*.h)
OBJECTS := $(SOURCES:.c=.o)
TARGET_EXECS := tests/test1

.PHONY: all clean

all: $(TARGET_EXECS)

tests/test1: tests/test1.o src/operations.o src/state.o

clean:
	rm -f $(OBJECTS) $(TARGET_EXECS)
