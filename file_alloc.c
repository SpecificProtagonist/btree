#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "btree.h"

// How many nodes to allocate at once
#define ALLOC_NODES_STEP 32

typedef struct {
    struct bt_alloc base;
    int file_descriptor;
    // The highest node id allocated so far + 1
    bt_node_id max_allocated;
    // File size in nodes
    bt_node_id file_size;
} file_alloc;




void tree_created(btree tree){}

bt_node_id new(void *this){
    file_alloc *a = (file_alloc*)this;
    
    a->max_allocated++;

    if(a->max_allocated > a->file_size){
        a->file_size += ALLOC_NODES_STEP;
        //TODO: proper error handling – store errno in alloc? 
        if((errno = posix_fallocate(a->file_descriptor, 0,
                    a->base.node_size * a->file_size))){
            perror("Failed to allocate file space");
            exit(1);
        }
    }
    return a->max_allocated-1;
}

void *load(btree tree, bt_node_id node){
    //TODO: somewhat expensive because of page faults → cache this
    void *mem = mmap(NULL, tree.alloc->node_size, PROT_READ|PROT_WRITE, MAP_SHARED,
            ((file_alloc*)tree.alloc)->file_descriptor, node*tree.alloc->node_size);
    //TODO: proper error handling – store errno in alloc? 
    if(mem == MAP_FAILED){
        perror("Map failed");
        exit(1);
    }
    return mem;
}

void unload(btree tree, void *node){
    munmap(node, tree.alloc->node_size);
}

void free_node(void *this, bt_node_id node){}

void tree_deleted(btree tree){}




bt_alloc_ptr btree_new_file_alloc(int fd, uint8_t** userdata, int userdata_size){

    int node_size = getpagesize();

    file_alloc *alloc = malloc(sizeof(file_alloc));
    alloc->base = (struct bt_alloc){
        tree_created,
        new,
        load,
        unload,
        free_node,
        tree_deleted,

        node_size
    };
    alloc->file_descriptor = fd;
    alloc->max_allocated = 0;

    struct stat filestat;
    fstat(fd, &filestat);
    alloc->file_size = filestat.st_size / node_size;
    
    return &alloc->base;
}

bt_alloc_ptr btree_load_file_alloc(int fd, uint8_t **userdata){
    fputs("Not implemented yet\n", stderr);
    exit(1);
}
