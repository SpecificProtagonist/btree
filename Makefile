DEFINES = 
OPT = $(DEFINES) -Wall -Wextra -pedantic-errors

# make some better recipes when there are more files to worry about

build: src/test.c src/btree.h src/btree-debug.h build/btree.o
	@gcc $(OPT) $^ -o build/test
	
test: build
	@./test

build/btree.o: src/btree.c src/btree.h
	@gcc $(OPT) -c $< -o $@

clean:
	@rm -rf build/*

.PHONY: build test clean
