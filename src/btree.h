#ifndef B_TREE_HEADER
#define B_TREE_HEADER

#include <stdbool.h>
#include <stdint.h>

// For now everything is in RAM, may change later
// Should a value be stored alongside the key? Can be added later
// The key type must be known at compile type

typedef
#ifdef BT_KEY_TYPE
    BT_KEY_TYPE
#else
    uint32_t
#endif
bt_key;

typedef struct btree btree;

btree* btree_new();

void btree_insert(btree*, bt_key key);

bool btree_contains(btree*, bt_key key);

void btree_traverse(btree*, void (*callback)(bt_key), bool reverse);

void btree_delete(btree*, bt_key key);

void btree_free(btree*);

#endif
