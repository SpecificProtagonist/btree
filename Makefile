# OPTIONS
# Can also be set from the commandline, e.g. make MAX_KEYS=420
# Maximum number of keys per node. Eachh interior node will contain MAX_KEYS+1 children
MAX_KEYS = 30
# Datatype of keys & values. If you override them, make sure to #define them in your code.
KEY_TYPE = uint32_t
VALUE_TYPE = void*


#TODO: test types with spaces, e.g. 'short int'
DEFINES = MAX_KEYS=$(MAX_KEYS) BT_KEY_TYPE=$(KEY_TYPE) BT_VALUE_TYPE=$(VALUE_TYPE)
OPT = -g -Wall -Wextra -Wno-missing-field-initializers -pedantic-errors $(addprefix -D,$(DEFINES))


# Build static library, there's no reason for a shared lib
static: build/btree.o
	@gcc $(OPT) btree.c -c -o build/btree.o
	@ar rcs build/libbtree.a build/btree.o

test: test.c
	@echo "Testing with MAX_KEYS=3"
	@make -s test_concrete VALUE_TYPE=uint32_t MAX_KEYS=3
	@echo "Testing with MAX_KEYS=40"
	@make -s test_concrete VALUE_TYPE=uint32_t MAX_KEYS=40
	@echo "Testing with MAX_KEYS=41"
	@make -s test_concrete VALUE_TYPE=uint32_t MAX_KEYS=41
	@echo "Test successful"

test_concrete: static
	@gcc $(OPT) test.c -Lbuild -lbtree -o build/test
	@build/test

build/btree.o: btree.c btree.h
	@gcc $(OPT) -c $< -o $@

clean:
	@rm -rf build/*

.PHONY: static test clean
