CC=gcc

CFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-parameter -pedantic-errors
LIBOBJ = btree.o ram_alloc.o file_alloc.o

# Build static library, there's no reason for a shared lib
release: CFLAGS += -O2
release: $(addprefix build/release/, $(LIBOBJ))
	@ar rcs build/release/libbtree.a $^

debug: CFLAGS += -g -Og
debug: $(addprefix build/debug/, $(LIBOBJ))
	@ar rcs build/debug/libbtree.a $^

test:
	@make -s _test VALUE_TYPE=uint32_t

_test: debug
	@$(CC) $(CFLAGS) test.c -Lbuild/debug -lbtree -o build/debug/test
	@build/debug/test
	@echo "Test successful"

build/release/%.o: %.c btree.h
	@$(CC) $(CFLAGS) -c $< -o $@

build/debug/%.o: %.c btree.h
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm build/release/*
	@rm build/debug/*

.PHONY: static test clean
