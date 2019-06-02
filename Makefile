# OPTIONS
# Can also be set from the commandline, e.g. make MAX_KEYS=420
# Datatype of keys & values. If you override them, make sure to #define them in your code.
#TODO: test types with spaces, e.g. 'short int'
KEY_TYPE=uint32_t
VALUE_TYPE=void*

CC=gcc

DEFINES = BT_KEY_TYPE=$(KEY_TYPE) BT_VALUE_TYPE=$(VALUE_TYPE)
CFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-parameter -pedantic-errors $(addprefix -D,$(DEFINES))


# Build static library, there's no reason for a shared lib
static: build/btree.o build/ram_alloc.o build/file_alloc_single.o build/file_alloc_multi.o
	@ar rcs build/libbtree.a $^

test:
	@make -s _test VALUE_TYPE=uint32_t

_test: static
	@$(CC) $(CFLAGS) test.c -Lbuild -lbtree -o build/test
	@build/test
	@echo "Test successful"

build/%.o: %.c btree.h
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf build/*

.PHONY: static test clean
