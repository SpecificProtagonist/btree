#include <stdlib.h>
#include "btree.h"


// Nothing to do
static void tree_created(void *this, btree *tree){}

// Allcate space
static bt_node_id new(void *this){
    // Maybe request memory aligned memory if power of 2?
    return (bt_node_id)malloc(((bt_allocator*)this)->node_size);
}

// Nothing to do beside cast, already in RAM
static void *load(void *this, btree *tree, bt_node_id node){
    return (void*) node;
}

// Nothing to do, node will stay in RAM
static void unload(void *this, btree *tree, bt_node_id node){}

// Deallocate node
static void free_node(void *this, bt_node_id node){
    free((void*)node);
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
