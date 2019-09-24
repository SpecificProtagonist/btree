#include <stdlib.h>
#include <errno.h>
#include "btree.h"

struct bt_ram_alloc {
    struct bt_alloc base;
    bt_error_callback error_callback;
};

// Allcate space
static bt_node_id new(void *this){
    // Replacing this with mmap(MAP_32BIT) would allow bt_nodes_id
    // to be 32 bit instead of 64
    void *node = malloc(((bt_alloc_ptr)this)->node_size);
    if(node==NULL){
        bt_error_callback callback = ((struct bt_ram_alloc*)this)->error_callback;
        if(callback)
            callback(this, errno);
        else {
            fputs("Error: Failed to allocate btree node, not enough RAM\n", stderr);
            exit(1);
        }
    }
    return (bt_node_id)node;
}

// Nothing to do beside cast, already in RAM
static void *load(btree tree, bt_node_id node){
    return (void*) node;
}

// Nothing to do, node will stay in RAM
static void unload(btree tree, void *node){}

// Deallocate node
static void free_node(void *this, bt_node_id node){
    free((void*)node);
}



bt_alloc_ptr btree_new_ram_alloc(uint16_t node_size, bt_error_callback error_callback){
    struct bt_ram_alloc *alloc = malloc(sizeof(struct bt_ram_alloc));
    alloc->base = (struct bt_alloc){
        new,
        load,
        unload,
        free_node,

        node_size
    };
    alloc->error_callback = error_callback;
    return (bt_alloc_ptr)alloc;
}
