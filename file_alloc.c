#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "btree.h"

// How much file space in nodes to allocate at once
#define ALLOC_NODES_STEP 32
// Maximum depth of the free blocks tree. This should be much more than enough.
#define MAX_FREE_DEPTH 26


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
    // Temporary buffer for nodes freed during node allcation
    bt_node_id freed_nodes[MAX_FREE_DEPTH];
    uint8_t freed_nodes_lenght;
    // Buffer of free nodes for the free_nodes tree to take from
    bt_node_id available_nodes[MAX_FREE_DEPTH];
    uint8_t available_nodes_lenght;
} helper_alloc;

typedef struct {
    struct bt_alloc base;
    int file_descriptor;
    // File size in nodes
    bt_node_id file_size;
    // Tree containing free blocks (todo: ranges instead of single blocks)
    btree free_tree;
    // Allocator of said tree
    helper_alloc free_tree_alloc;
    // Pointer to userdata of root node, stores
    // the highest node id allocated so far + 1
    bt_node_id *root_userdata;
} file_alloc;

#define MAIN_ALLOC_PTR(h_alloc) (file_alloc*)((char*)h_alloc + \
        ((char*)((file_alloc*)NULL) - (char*)&((file_alloc*)NULL)->free_tree_alloc))



static bt_node_id helper_new_node(void *this){
    helper_alloc *alloc = (helper_alloc*)this;
    return alloc->available_nodes[--alloc->available_nodes_lenght];
}


static void *load_from_alloc(file_alloc*, bt_node_id);

static void *helper_load(btree tree, bt_node_id node){
    return load_from_alloc(MAIN_ALLOC_PTR(tree.alloc), node);
}


static void unload_from_alloc(file_alloc *alloc, void *node);

static void helper_unload(btree tree, void *node){
    unload_from_alloc(MAIN_ALLOC_PTR(tree.alloc), node);
}


static void free_node(void *this, bt_node_id node);

static void helper_free_node(void *this, bt_node_id node){
    helper_alloc *alloc = (helper_alloc*)this;
    alloc->freed_nodes[alloc->freed_nodes_lenght++] = node;
}



static bool callback_get_first_node(const void *key, void *value, void *param){
    *(bt_node_id*)param = *(bt_node_id*)key;
    return true;
}

static bt_node_id new(void *this){
    file_alloc *a = (file_alloc*)this;
   
    if(btree_is_empty(a->free_tree)){
        (*(bt_node_id*)a->root_userdata)++;
        if(*(bt_node_id*)a->root_userdata > a->file_size){
            a->file_size += ALLOC_NODES_STEP;
            //TODO: proper error handling – store errno in alloc? 
            if((errno = posix_fallocate(a->file_descriptor, 0,
                        a->base.node_size * a->file_size))){
                perror("Failed to allocate file space");
                exit(1);
            }
        }
        return *(bt_node_id*)a->root_userdata-1;
    } else {
        bt_node_id new_node;
        btree_traverse(a->free_tree, callback_get_first_node, &new_node, false);
        btree_remove(a->free_tree, &new_node, NULL);
        // After a remove, the free_tree may have shrunk
        for(int i = a->free_tree_alloc.freed_nodes_lenght; i --> 0;)
            btree_insert(a->free_tree, a->free_tree_alloc.freed_nodes+i, NULL);
        a->free_tree_alloc.freed_nodes_lenght = 0;
        return new_node;
    }
}



static void *load_from_alloc(file_alloc *alloc, bt_node_id node){
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

static void *load(btree tree, bt_node_id node){
    return load_from_alloc((file_alloc*)tree.alloc, node);
}

static void unload_from_alloc(file_alloc *alloc, void *node){
    munmap(node, alloc->base.node_size);
}

static void unload(btree tree, void *node){
    unload_from_alloc((file_alloc*)tree.alloc, node);
}

static void free_node(void *this, bt_node_id node){
    file_alloc *alloc = (file_alloc*)this;
    if(alloc->free_tree_alloc.available_nodes_lenght<MAX_FREE_DEPTH){
        alloc->free_tree_alloc.available_nodes[alloc->free_tree_alloc.available_nodes_lenght++] = node;
    } else {
        btree_insert(alloc->free_tree, &node, NULL);
    }
}



// Initialize a new file_alloc as far as both creation and loading from file require
static file_alloc *get_alloc_base(int fd){
    // The size of each allocation. A page is usually 4kb in size.
    // Maybe instead make this a compile option?
    int node_size = getpagesize();

    // Construct struct describing the allocator
    file_alloc *alloc = calloc(1, sizeof(file_alloc));
    alloc->base = (struct bt_alloc){
        new,
        load,
        unload,
        free_node,

        node_size
    };
    alloc->file_descriptor = fd;

    // Store the file size, else every allocation (when the free nodes tree is empty)
    // would require calling fstat
    struct stat filestat;
    fstat(fd, &filestat);
    alloc->file_size = filestat.st_size / node_size;

    // Construct allocator for the free nodes tree,
    alloc->free_tree_alloc.base = (struct bt_alloc){
        helper_new_node,
        helper_load,
        helper_unload,
        helper_free_node,

        alloc->base.node_size
    };
    alloc->free_tree_alloc.freed_nodes_lenght = 0;

    return alloc;
}


bt_alloc_ptr btree_new_file_alloc(int fd, void** userdata, int userdata_size){
    // Basic init shared with btree_load_file_alloc
    file_alloc *alloc = get_alloc_base(fd);
    
    // Make sure the file has minimum enough size for the root
    // TODO: proper error handling – store errno in alloc? 
    if(!alloc->file_size) {
        alloc->file_size = 2;
        if((errno = posix_fallocate(fd, 0,
                    alloc->base.node_size * alloc->file_size))){
            perror("Failed to allocate file space");
            exit(1);
        }
    }

    // The free nodes tree will not have any values associated with the keys.
    // Also store userdata and max_allocated in the root node.
    alloc->free_tree = btree_create((bt_alloc_ptr)&alloc->free_tree_alloc,
                            sizeof(bt_node_id), 0, NULL,
                            userdata_size + sizeof(bt_node_id));

    // The root will be set to node 0
    alloc->free_tree_alloc.available_nodes[0] = 0;
    alloc->free_tree_alloc.available_nodes_lenght = 1;

    alloc->root_userdata = btree_load_userdata(alloc->free_tree);
    // The first node is taken by the free nodes tree root
    *(bt_node_id*)alloc->root_userdata = 1;
    // Real userdate comes after max_allocated
    if(userdata)
        *userdata = (char*)btree_load_userdata(alloc->free_tree)+sizeof(bt_node_id);

    return (bt_alloc_ptr)alloc;
}



bt_alloc_ptr btree_load_file_alloc(int fd, void **userdata){
    file_alloc *alloc = get_alloc_base(fd);

    alloc->free_tree = (btree){
        .alloc = (bt_alloc_ptr)&alloc->free_tree_alloc,
        .root = 0,
        .compare = memcmp
    };

    alloc->root_userdata = btree_load_userdata(alloc->free_tree);
    // TODO: worry about max_allocated allignment
    if(userdata)
        *userdata = (uint8_t*)alloc->root_userdata + sizeof(bt_node_id);

    return (bt_alloc_ptr)alloc;
}
