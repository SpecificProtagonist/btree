#ifndef B_TREE_HEADER
#define B_TREE_HEADER

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// For now everything is in RAM, may change later
// Should a value be stored alongside the key? Can be added later
// The key type must be known at compile type

// Uähh! No generics, I want C++!
typedef
#ifdef BT_KEY_TYPE
    BT_KEY_TYPE
#else
    uint32_t
#endif
bt_key;

typedef
#ifdef BT_VALUE_TYPE
    BT_VALUE_TYPE
#else
    void*
#endif
bt_value;

typedef struct btree btree;

btree* btree_new();

// Inserts the key and corresponding value, 
// returns true if key was already present.
bool btree_insert(btree*, bt_key, bt_value);

// Checks whether the tree contains the key
bool btree_contains(btree*, bt_key);

// Traverses tree, calling callback() with each element and id,
// the later of which allows parallel calls from multiple threads;
// the return value of callback will be stored.
void btree_traverse(btree*, 
        bt_value(*callback)(bt_key, bt_value, void*),
        void* id, bool reverse);

// Remove the key from the tree
void btree_delete(btree*, bt_key);

// Deallocates all memory taken up by the tree
void btree_free(btree*);

// Prints out a textual representation of the btree (intended for a monospace font)
// to stream. Uses hexadecimal format for keys and pointer format for values,
// only prints values if print_value; expects utf-8 locale.
void btree_debug_print(FILE *stream, btree*, bool print_value);

#endif
