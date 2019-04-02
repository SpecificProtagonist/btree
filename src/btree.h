#ifndef B_TREE_HEADER
#define B_TREE_HEADER

#include <stdbool.h>

//TODO: proper type
typedef void* btree;

btree* btree_new();

// int should be sensible for key type,
// Should a value be stored alongside the key? Can be added later

void btree_insert(btree*, int key);

bool btree_contains(btree*, int key);

void btree_traverse(btree*, void (*callback)(int), bool reverse);

void btree_delete(btree*, int key);

void btree_free(btree*);

#endif
