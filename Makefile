# OPTIONS
# Can also be set from the commandline, e.g. make MAX_KEYS=420
# Datatype of keys & values. If you override them, make sure to #define them in your code.
#TODO: test types with spaces, e.g. 'short int'
KEY_TYPE=uint32_t
VALUE_TYPE=void*

CC=gcc

DEFINES = BT_KEY_TYPE=$(KEY_TYPE) BT_VALUE_TYPE=$(VALUE_TYPE)
CFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-parameter -pedantic-errors $(addprefix -D,$(DEFINES))
LIBOBJ = btree.o ram_alloc.o file_alloc_single.o file_alloc_multi.o

# Build static library, there's no reason for a shared lib
release: CFLAGS += -O2
release: $(addprefix build/release/, $(LIBOBJ))
	@ar rcs build/release/libbtree.a $^

debug: CFLAGS += -Og -g
debug: $(addprefix build/debug/, $(LIBOBJ))
	@ar rcs build/debug/libbtree.a $^

test:
	@make -s _test VALUE_TYPE=uint32_t

_test: debug
	@$(CC) $(CFLAGS) test.c -Lbuild/debug -lbtree -o build/test
	@build/test
	@echo "Test successful"

build/release/%.o: %.c btree.h
	@$(CC) $(CFLAGS) -c $< -o $@

build/debug/%.o: %.c btree.h
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm build/release/*
	@rm build/debug/*

.PHONY: static test clean
