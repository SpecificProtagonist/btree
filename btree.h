#ifndef B_TREE_HEADER
#define B_TREE_HEADER

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// B-Trees are balanced trees, typicaly with a high fanout,
// often used for file systems and other databases.

typedef struct btree btree;
typedef struct bt_alloc *bt_alloc_ptr;
typedef uint64_t bt_node_id;


// Creates a new allocator that keeps each entire trees in RAM,
// can be freed with free().
// TODO: recommend default node_size (requires benchmark)
bt_alloc_ptr btree_new_ram_alloc(uint16_t node_size);


// Creates a new allocator that keeps trees in a file.
// Trees (or other data) already present there will be overriden.
// Can be freed with free().
// A small amount of data, e.g a bt_node_id, can be stored alongside the allocator,
// and a pointer to it will be stored in the location userdata points to.
bt_alloc_ptr btree_new_file_alloc(int fd, uint8_t **userdata, int userdata_size);

// Loads the allocator created with btree_new_file_alloc() from file
bt_alloc_ptr btree_load_file_alloc(int fd, uint8_t **userdata);

// To load an existing btree, simply initialize the following structure
// with the correct values.
struct btree {
    bt_alloc_ptr alloc;
    bt_node_id root;
    int (*compare)(const void*, const void*, size_t);
};





// The following functions are multithreading safe only if no simultaneous
// operations are performed on btrees created with the same allocator


// Creates a new b-tree from the given allocator.
// userdata_size specifies the size of custom data (if any) stored along
// side the tree (should be much smaller than the allocators node_size).
// You can specify the function for comparing keys. If NULL, memcmp is used.
btree btree_create(bt_alloc_ptr, uint8_t key_size, uint8_t value_size, 
        int (*compare)(const void *key1, const void *key2, size_t size), 
        uint16_t userdata_size);

// Gets a pointer to the userdata stored alongside the tree.
// This doesn't have a guaranteed alignment.
void *btree_load_userdata(btree);

// Indicate that the userdata pointer isn't in use anymore
void btree_unload_userdata(btree, void *userdata);

// Inserts the key and corresponding value, 
// returns true if key was already present.
bool btree_insert(btree, const void *key, const void *value);

// Checks whether the tree contains the key
bool btree_contains(btree, const void *key);

// Retrieves the corresponding value and, if found, stores it in *value.
// If success is not null, stores whether the key was found.
void btree_get(btree, const void *key, void *value, bool *success);

// Traverses tree, calling callback() with a pointer to each key&value and params.
// If callback return true, end traversal early and return true, else return false.
bool btree_traverse(btree, 
        bool (*callback)(const void *key, void *value, void *param),
        void* params, bool reverse);

// Remove the key from the tree, return true if the tree did contain it, else false.
bool btree_remove(btree, const void *key);

// Deletes a tree
void btree_delete(btree);

// Prints out a textual representation of the btree (intended for a monospace font)
// to stream. Uses hexadecimal format for keys and pointer format for values,
// only prints values if print_value; expects utf-8 locale and VT1000.
void btree_debug_print(FILE *stream, btree, bool print_value);





// Allocators manage memory for b-trees. You can define your own, 
// but the inbuild ones should be sufficient in most cases.
struct bt_alloc {
    // Indicate that a new tree has been created
    void (*tree_created)(btree);
    // Allocates space for a new node of size node_size.
    // This may also be used to store data other than tree nodes.
    // Node id 0 marks invalid node.
    bt_node_id (*new)(void *this);
    // Make sure that the node is in memory, 
    // which means doing nothing in case of bt_ram_allocator.
    void *(*load)(btree, bt_node_id node);
    // Indicate that the node doesn't have to be kept in memory anymore.
    void (*unload)(btree, void *node);
    // Deallocates a node (may not be called when loaded)
    void (*free)(void* this, bt_node_id node);
    // Indicate that the b-tree has been deleted
    void (*tree_deleted)(btree);

    // Size of a node in bytes
    uint16_t node_size;
};

#endif
