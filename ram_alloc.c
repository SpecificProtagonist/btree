#include <stdlib.h>
#include "btree.h"


// Nothing to do
static void tree_created(void *this, btree *tree){}

// Allcate space
static void *new(void *this){
    return malloc(((bt_allocator*)this)->node_size);
}

// Nothing to do, already in RAM
static void load(void *this, void *node){}

// Nothing to do, node will stay in RAM
static void unload(void *this, void *node){}

// Deallocate node
static void free_node(void *this, void *node){
    free(node);
}

// Nothing to do
static void tree_deleted(void *this, btree *tree){}



bt_allocator *btree_new_ram_alloc(uint16_t node_size){
    bt_allocator *ram_alloc = malloc(sizeof(bt_allocator));
    *ram_alloc = (bt_allocator){
        tree_created,
        new,
        load,
        unload,
        free_node,
        tree_deleted,
        node_size
    };
    return ram_alloc;
}
