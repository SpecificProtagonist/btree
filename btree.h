#ifndef B_TREE_HEADER
#define B_TREE_HEADER

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// B-Trees are balanced trees, typicaly with a high fanout,
// often used for file systems and other databases.

// This implementation offers no multithreading support.

// The key and value types must be known at compile time.
// A way around this would be combining makro with storing 
// the size of both types in the struct btree.
typedef
#ifdef BT_KEY_TYPE
    BT_KEY_TYPE
#else
    uint32_t
#endif
bt_key;

typedef
#ifdef BT_VALUE_TYPE
    BT_VALUE_TYPE
#else
    void*
#endif
bt_value;

typedef struct btree btree;



// Allocators manage memory for b-trees. You can define your own, 
// but the inbuild ones should be sufficient in most cases.
// You shouldn't call any member function yourself and 
// should only store it as a pointer (as not to remove any extra data).
typedef uint64_t bt_node_id;
typedef struct {
    // Indicate that a new tree has been created
    void (*tree_created)(void *this, btree*);
    // Allocates space for a new node of size node_size
    bt_node_id (*new)(void *this);
    // Make sure that the node is in memory, 
    // which means doing nothing in case of bt_ram_allocator.
    // Will be called once with NULL as the tree during btree_create().
    void *(*load)(void *this, btree*, bt_node_id node);
    // Indicate that the node doesn't have to be kept in memory anymore.
    void (*unload)(void *this, btree*, bt_node_id node);
    // Deallocates a node
    void (*free)(void* this, bt_node_id node);
    // Indicate that the b-tree has been deleted
    void (*tree_deleted)(void *this, btree*);

    // Size of a node in byte
    uint16_t node_size;
} bt_allocator;

// Creates a new allocator that keeps each entire trees in RAM,
// can be freed with free().
// TODO: recommend default node_size (requires benchmark)
bt_allocator *btree_new_ram_alloc(uint16_t node_size);

// Creates a new allocator that keeps a tree in a file.
// A tree (or other data) already present there will be overriden.
// This allocator can only supply a single tree, to create another one
// the previous one has to be deleted.
// Can be freed with free().
bt_allocator *btree_new_file_alloc_single(FILE *file);

// Loads the allocator created with bt_new_file_alloc_single() from file.
bt_allocator *btree_load_file_alloc_single(FILE *file);

// Creates a new allocator that keeps trees in a file.
// Trees (or other data) already present there will be overriden.
// Can be freed with free().
bt_allocator *btree_new_file_alloc_multi(FILE *file);

// Loads the allocator created with bt_new_file_alloc_multi() from file
bt_allocator *btree_load_file_alloc_multi(FILE *file);

// Gets a unique id for a tree created with a file_alloc_multi
uint32_t btree_file_alloc_multi_get_id(btree*);

// Get a tree by id from a file_alloc_multi
btree *btree_file_alloc_multi_get_tree(uint32_t id);



// Creates a new b-tree from the given allocator.
// userdata_size specifies the size the custom data stored alongside the tree
// (should me much smaller than the allocators node_size).
btree *btree_create(bt_allocator*, uint16_t userdata_size);

// Gets a pointer to the userdata stored alongside the tree,
// the size of which depends on the allocator.
// TODO: load/unload tree
void *btree_userdata_pointer(btree*);

// Inserts the key and corresponding value, 
// returns true if key was already present.
bool btree_insert(btree*, bt_key, bt_value);

// Checks whether the tree contains the key
bool btree_contains(btree*, bt_key);

// Retrieves the corresponding value.
// If success is not null, stores whether the key was found.
bt_value btree_get(btree*, bt_key, bool *success);

// Retrieves the corresponding value or return default_value if not found.
bt_value btree_get_or_default(btree*, bt_key, bt_value default_value);

// Traverses tree, calling callback() with each element and params
// and storing back the return value.
void btree_traverse(btree*, 
        bt_value(*callback)(bt_key, bt_value, void*),
        void* params, bool reverse);

// Remove the key from the tree, return true if the tree did contain it, else false.
bool btree_remove(btree*, bt_key);

// Deletes a tree
void btree_delete(btree*);

// Prints out a textual representation of the btree (intended for a monospace font)
// to stream. Uses hexadecimal format for keys and pointer format for values,
// only prints values if print_value; expects utf-8 locale and VT1000.
void btree_debug_print(FILE *stream, btree*, bool print_value);

#endif
