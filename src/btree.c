#include <stdlib.h>
#include <stdio.h>
#include "btree.h"

// store order as property of the btree or as compile time option?
// 32 or so might be sensible if kept in RAM, 4 good for testing
#ifndef MAX_CHILDREN
#define MAX_CHILDREN 4
#endif
#define MIN_CHILDREN ((MAX_CHILDREN+1)/2)

typedef struct bt_node bt_node;
struct bt_node {
    uint8_t num_keys;
    bt_key keys[MAX_CHILDREN-1];
    bt_node* children[MAX_CHILDREN];
};

typedef struct {
    uint8_t num_keys;
    bt_key keys[MAX_CHILDREN-1];
} bt_node_leaf;

struct btree {
   bt_node* root;
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

void debug_print(bt_node* node, int height, int max_height){
    for(int i = 0; i < node->num_keys+1; i++){
        if(height>0){
            debug_print(node->children[i], height-1, height);
        }
        if(i<node->num_keys){
            for(int s = max_height-height; s --> 0; printf("\t"));
            printf("%d\n", node->keys[i]);
        }
    }
}

void btree_debug_print(btree* tree){
    if(tree->root)
        debug_print(tree->root, tree->height, tree->height);
    else
        puts("EMPTY");
}
