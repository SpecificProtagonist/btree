#include <stdlib.h>
#include "btree.h"

// store order as property of the btree or as compile time option?
// 32 or so might be sensible if kept in RAM, 4 good for testing
#ifndef MAX_CHILDREN
#define MAX_CHILDREN 4
#endif
#define MIN_CHILDREN ((MAX_CHILDREN+1)/2)

typedef struct bt_node* node;
struct bt_node {
    uint8_t num_children;
    bt_key keys[MAX_CHILDREN-1];
    node* children[MAX_CHILDREN];
};

typedef struct {
    uint8_t num_children;
    bt_key keys[MAX_CHILDREN-1];
} bt_node_leaf;

struct btree {
   node root;
   int8_t height;
};

btree* btree_new(){
    return calloc(sizeof(btree), 1);
}

void btree_insert(btree*, bt_key key);

bool btree_contains(btree* tree, bt_key key);

void btree_traverse(btree*, void (*callback)(bt_key), bool reverse);

void btree_delete(btree*, bt_key key);

void btree_free(btree*);

