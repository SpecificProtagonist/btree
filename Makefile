# OPTIONS
# Can also be set from the commandline, e.g. make MAX_KEYS=40
# Maximum number of keys per node. Eachh interior node will contain MAX_KEYS+1 children
MAX_KEYS = 3
# Datatype of keys & values. If you override them, make sure to #define them in your code.
KEY_TYPE = uint32_t
VALUE_TYPE = void*


#TODO: test types with spaces, e.g. 'short int'
DEFINES = MAX_KEYS=$(MAX_KEYS) BT_KEY_TYPE=$(KEY_TYPE) BT_VALUE_TYPE=$(VALUE_TYPE)
OPT = -g -Wall -Wextra -Wno-missing-field-initializers -pedantic-errors $(addprefix -D,$(DEFINES))

# make some better recipes when there are more files to worry about

build: src/test.c src/btree.h build/btree.o
	@gcc $(OPT) $^ -o build/test
	
test: build
	@build/test

build/btree.o: src/btree.c src/btree.h
	@gcc $(OPT) -c $< -o $@

clean:
	@rm -rf build/*

.PHONY: build test clean
