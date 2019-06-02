#include <stdlib.h>
#include "btree.h"


// Nothing to do
static void tree_created(btree tree){}

// Allcate space
static bt_node_id new(void *this){
    // Replacing this with mmap(MAP_32BIT) would allow bt_nodes_id
    // to be 32 bit instead of 64
    return (bt_node_id)malloc(((bt_alloc_ptr)this)->node_size);
}

// Nothing to do beside cast, already in RAM
static void *load(btree tree, bt_node_id node){
    return (void*) node;
}

// Nothing to do, node will stay in RAM
static void unload(btree tree, bt_node_id node){}

// Deallocate node
static void free_node(void *this, bt_node_id node){
    free((void*)node);
}

// Nothing to do
static void tree_deleted(btree tree){}



bt_alloc_ptr btree_new_ram_alloc(uint16_t node_size){
    bt_alloc_ptr ram_alloc = malloc(sizeof(struct bt_alloc));
    *ram_alloc = (struct bt_alloc){
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
