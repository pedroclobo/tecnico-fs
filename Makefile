CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -pthread

INCLUDE_DIRS := fs .
INCLUDES = $(addprefix -I, $(INCLUDE_DIRS))

SOURCES := $(wildcard fs/*.c)
HEADERS := $(wildcard fs/*.h)
OBJECTS := $(SOURCES:.c=.o)

TEST_FILES := $(wildcard tests/*.c)
TEST_OBJECTS := $(TEST_FILES:.c=.o)
TESTS := $(TEST_FILES:.c=)

all:: $(OBJECTS)

test%: $(OBJECTS) test%.o
	$(CC) $(CFLAGS) $^ -o $@

tests:: $(OBJECTS) $(TEST_OBJECTS) $(TESTS)
	@for test in $(TESTS); do \
		./$$test; \
	done

clean::
	rm -f $(OBJECTS) $(TARGET_EXECS) $(TESTS) $(TEST_OBJECTS) *.txt *.zip

zip::
	zip proj.zip Makefile fs/*.c fs/*.h tests/test-p-01.c tests/test-p-02.c tests/test-p-03.c
