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
typedef int (*bt_key_comp)(const void*, const void*, size_t);

// Some operations may result in underlying IO errors, e.g. running out of memory.
// You can set an error callback taking the errno (assuming bt_alloc_ptr alloc),
// if you don't the library will print an error to stderr and exit.
typedef void (*bt_error_callback)(bt_alloc_ptr, int error);



// Creates a new allocator that keeps each entire trees in RAM,
// can be freed with free().
// TODO: recommend default node_size (requires benchmark)
bt_alloc_ptr btree_new_ram_alloc(uint16_t node_size, bt_error_callback);

// Creates a new allocator that keeps trees in a file.
// Trees (or other data) already present there will be overriden.
// A small amount of data, e.g a bt_node_id, can be stored alongside the allocator,
// and a pointer to it will be stored in the location userdata points to.
// If creation fails, NULL is returned and errno is set.
bt_alloc_ptr btree_new_file_alloc(int fd, void **userdata, int userdata_size, bt_error_callback);

// Loads the allocator created with btree_new_file_alloc() from file.
// If creation fails, NULL is returned and errno is set.
bt_alloc_ptr btree_load_file_alloc(int fd, void **userdata, bt_error_callback);

// To load an existing btree, simply initialize the following structure
// with the correct values. If you created the tree with compare==NULL,
// you'll have to set compare to memcmp.
struct btree {
    bt_alloc_ptr alloc;
    bt_node_id root;
    bt_key_comp compare;
};






// The following functions are multithreading safe only if no simultaneous
// operations are performed on btrees created with the same allocator


// Creates a new b-tree from the given allocator.
// userdata_size specifies the size of custom data (if any) stored along
// side the tree (should be much smaller than the allocators node_size).
// This is useful primarily for btrees stored in a file.
// You can specify the function for comparing keys, which is useful if you want
// to be able to traverse the tree in a specific order. If NULL, memcmp is used.
btree btree_create(bt_alloc_ptr, uint8_t key_size, uint8_t value_size, 
        bt_key_comp, uint16_t userdata_size);

// Gets a pointer to the userdata stored alongside the tree.
// This doesn't have a guaranteed alignment.
void *btree_load_userdata(btree);

// Indicate that the userdata pointer isn't in use anymore
void btree_unload_userdata(btree, void *userdata);

// Inserts the key and corresponding value, 
// returns true if key was already present.
bool btree_insert(btree, const void *key, const void *value);

// Checks whether the btree is empty
bool btree_is_empty(btree);

// Checks whether the tree contains the key
bool btree_contains(btree, const void *key);

// Retrieves the corresponding value and, if found, stores it in *value_out.
// Returns whether the key was found.
bool btree_get(btree, const void *key, void *value_out);

// Traverses tree, calling callback() with a pointer to each key&value and params.
// If callback return true, end traversal early and return true, else return false.
bool btree_traverse(btree, 
        bool (*callback)(const void *key, void *value, void *param),
        void* params, bool reverse);

// Remove the key from the tree, store the corresponding value (if value_out!=NULL).
// Return true if the tree did contain the key, else false.
bool btree_remove(btree, const void *key, void *value_out);

// Deletes a tree
void btree_delete(btree);

// Prints out a textual representation of the btree (intended for a monospace font)
// to stream. Expects utf-8 locale and VT1000. A function to print keys/values can
// be specified; if it is NULL, both are printed in hex format;
typedef void (*bt_printer_t)(FILE*, const void *key, const void *value, void *param);
void btree_debug_print(FILE *stream, btree, bt_printer_t, void *param);





// Allocators manage memory for b-trees. You can define your own, 
// but the inbuild ones should be sufficient in most cases.
struct bt_alloc {
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

    // Size of a node in bytes
    uint16_t node_size;
};

#endif
