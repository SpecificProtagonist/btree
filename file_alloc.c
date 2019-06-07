#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "btree.h"

// How many nodes to allocate at once
#define ALLOC_NODES_STEP 32
// Maximum depth of the free blocks tree. This should be much more than enough.
#define MAX_FREE_DEPTH 26


typedef struct file_alloc file_alloc;

// 
// Free space will be stored in a btree with root node id 0
// and value size 0
// Problem: How to allocate/deallocate nodes for that tree?
// If we use the same tree for this (buffering the requested/freed pages
// because we can't manipulate the tree while another operation is afoot),
// can this cause a free node → free-node-tree requests node → alloc node
// → free-node-tree shrinks → repeat loop?
// Solution: Buffer, put in there if not full, free_tree takes from that
//

typedef struct {
    struct bt_alloc base;
    file_alloc *main;
    // Temporary buffer for nodes freed during node allcation
    bt_node_id freed_nodes[MAX_FREE_DEPTH];
    uint8_t freed_nodes_lenght;
    // Buffer of free nodes for the free_nodes tree to take from
    bt_node_id available_nodes[MAX_FREE_DEPTH];
    uint8_t available_nodes_lenght;
} helper_alloc;

struct file_alloc {
    struct bt_alloc base;
    int file_descriptor;
    // The highest node id allocated so far + 1
    bt_node_id max_allocated;
    // File size in nodes
    bt_node_id file_size;
    // Tree containing free blocks (todo: ranges instead of single blocks)
    btree free_tree;
    // Allocator of said tree
    helper_alloc *free_tree_alloc;
};



bt_node_id helper_new_node(void *this){
    helper_alloc *alloc = (helper_alloc*)this;
    printf("New helper node (avalable: %d)\n", alloc->available_nodes_lenght);
    return alloc->available_nodes[--alloc->available_nodes_lenght];
}

bt_node_id helper_initial_node(void *this){
    helper_alloc *alloc = (helper_alloc*)this;
    alloc->base.new = helper_new_node;
    return 0;
}

void *load_from_alloc(file_alloc*, bt_node_id);

void *helper_load(btree tree, bt_node_id node){
    return load_from_alloc(((helper_alloc*)tree.alloc)->main, node);
}

void unload_from_alloc(file_alloc *alloc, void *node);

void helper_unload(btree tree, void *node){
    unload_from_alloc(((helper_alloc*)tree.alloc)->main, node);
}

void free_node(void *this, bt_node_id node);

void helper_free_node(void *this, bt_node_id node){
    helper_alloc *alloc = (helper_alloc*)this;
    alloc->freed_nodes[alloc->freed_nodes_lenght++] = node;
}

helper_alloc *get_helper_alloc(file_alloc *main){
    helper_alloc *alloc = malloc(sizeof(helper_alloc));
    alloc->base = (struct bt_alloc){
        helper_initial_node,
        helper_load,    // TODO: load all nodes into RAM,
        helper_unload,  //       don't unload them
        helper_free_node,

        main->base.node_size
    };
    alloc->main = main;
    alloc->freed_nodes_lenght = 0;
    return alloc;
}




bool callback_get_first_node(const void *key, void *value, void *param){
    *(bt_node_id*)param = *(bt_node_id*)key;
    return true;
}

bt_node_id new(void *this){
    file_alloc *a = (file_alloc*)this;
   
    if(btree_is_empty(a->free_tree)){
        printf("New main node (max_allocated: %ld)\n", a->max_allocated);
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
    } else {
        printf("New main node from free_tree\n");
        bt_node_id new_node;
        btree_traverse(a->free_tree, callback_get_first_node, &new_node, false);
        btree_remove(a->free_tree, &new_node);
        // After a remove, the free_tree may have shrunk
        for(int i = a->free_tree_alloc->freed_nodes_lenght; i --> 0;)
            btree_insert(a->free_tree, a->free_tree_alloc->freed_nodes+i, NULL);
        a->free_tree_alloc->freed_nodes_lenght = 0;
        return new_node;
    }
}



void *load_from_alloc(file_alloc *alloc, bt_node_id node){
    //TODO: somewhat expensive because of page faults → cache this
    void *mem = mmap(NULL, alloc->base.node_size, PROT_READ|PROT_WRITE, MAP_SHARED,
            ((file_alloc*)alloc)->file_descriptor, node*alloc->base.node_size);
    //TODO: proper error handling – store errno in alloc? 
    if(mem == MAP_FAILED){
        perror("Map failed");
        exit(1);
    }
    return mem;
}

void *load(btree tree, bt_node_id node){
    return load_from_alloc((file_alloc*)tree.alloc, node);
}

void unload_from_alloc(file_alloc *alloc, void *node){
    munmap(node, alloc->base.node_size);
}

void unload(btree tree, void *node){
    unload_from_alloc((file_alloc*)tree.alloc, node);
}

void free_node(void *this, bt_node_id node){
    file_alloc *alloc = (file_alloc*)this;
    if(alloc->free_tree_alloc->available_nodes_lenght<MAX_FREE_DEPTH){
        alloc->free_tree_alloc->available_nodes[alloc->free_tree_alloc->available_nodes_lenght++] = node;
    } else {
        btree_insert(alloc->free_tree, &node, NULL);
    }
}




bt_alloc_ptr btree_new_file_alloc(int fd, uint8_t** userdata, int userdata_size){

    int node_size = getpagesize();

    file_alloc *alloc = malloc(sizeof(file_alloc));
    alloc->base = (struct bt_alloc){
        new,
        load,
        unload,
        free_node,

        node_size
    };
    alloc->file_descriptor = fd;
    alloc->max_allocated = 1;

    struct stat filestat;
    fstat(fd, &filestat);
    alloc->file_size = filestat.st_size / node_size;

    alloc->free_tree_alloc = get_helper_alloc(alloc);
    alloc->free_tree = btree_create((bt_alloc_ptr)alloc->free_tree_alloc,
                            sizeof(bt_node_id), 0, NULL, userdata_size);
    //TODO: store available_nodes

    return &alloc->base;
}

bt_alloc_ptr btree_load_file_alloc(int fd, uint8_t **userdata){
    fputs("Not implemented yet\n", stderr);
    exit(1);
}
