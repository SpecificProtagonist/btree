# OPTIONS
# Can also be set from the commandline, e.g. make MAX_KEYS=420
# Datatype of keys & values. If you override them, make sure to #define them in your code.
#TODO: test types with spaces, e.g. 'short int'
KEY_TYPE=uint32_t
VALUE_TYPE=void*
# Page size should almost always be 4KiB
PAGE_SIZE=4096

CC=gcc

DEFINES = BT_KEY_TYPE=$(KEY_TYPE) BT_VALUE_TYPE=$(VALUE_TYPE) PAGE_SIZE=$(PAGE_SIZE)
CFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -pedantic-errors $(addprefix -D,$(DEFINES))


# Build static library, there's no reason for a shared lib
static: build/btree.o
	@$(CC) $(CFLAGS) btree.c -c -o build/btree.o
	@ar rcs build/libbtree.a build/btree.o

test:
	@make -s _test VALUE_TYPE=uint32_t

_test: static
	@$(CC) $(CFLAGS) test.c -Lbuild -lbtree -o build/test
	@build/test
	@echo "Test successful"

build/btree.o: btree.c btree.h
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf build/*

.PHONY: static test clean
