#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "btree.h"

/*************
 * DATATYPES *
 *************/

typedef void bt_node;

// Inserting/deleting keys/values/children is O(MAX_KEYS),
// so maybe use binary tree in array representation instead of array?
// Would be more complex though.
/// Node structure
/*  int16_t   num_keys
 *  int16_t   max_keys
 *  {key, value}    pairs[max_keys]
 * // only in interior nodes:
 *  bt_node_id children[max_keys+1]
 */

// Tree metadata, kept in a node
typedef struct {
    // Offset of the root node from the start of the btree_data in bytes
    // (the root node will be located on same page/node).
    // Can be either a leaf or an interior node, so it's max_keys
    // will be calculated for the later.
    // Since there is less space than on other nodes, if the root gets too
    // large, this only contains the bt_node_id of the actual root
    uint16_t root_offset;
    // Maximum number of keys for each key type (will differ for root)
    uint16_t max_interior_keys;
    uint16_t max_leaf_keys;
    // Height of the tree. -1 Means empty tree, 0 means the root is a leaf.
    int8_t height;
    // Size of key & value datatypes in bytes
    uint8_t key_size;
    uint8_t value_size;
    // Custom data (variable length) stored alongside tree
    char userdata[1];
} btree_data;

// Small structure passed amoung internal functions,
// contains metadata neccessary for managing nodes
// Maybe make thread-local variables instead?
typedef struct {
    btree tree;
    uint8_t key_size;
    uint8_t pair_size;
} tree_param;




/**********
 * MAKROS *
 **********/

// Accessor macros since nodes aren't structs
// These require the variable tree of type tree_param.
# define NUM_KEYS(node) (*(int16_t*)node)
# define MAX_KEYS(node) (*((int16_t*)node+1))
# define MIN_KEYS(node) (MAX_KEYS(node)/2)
# define PAIRS(node)    ((void*)((int16_t*)node+2))
# define PAIR(node, i)  ((uint8_t*)PAIRS(node)+(i)*tree.pair_size)
# define VALUE(pair) (pair+tree.key_size)
# define CHILDREN(node) ((bt_node_id*)(((char*)PAIRS(node))\
                            +tree.pair_size*MAX_KEYS(node)))
# define CHILD(node, i) ((uint8_t*)CHILDREN(node)+(i)*sizeof(bt_node_id))

# define ROOT(tree_data) ((bt_node*)((char*)tree_data+tree_data->root_offset))

# define LOAD(node_id) (tree.tree.alloc->load(tree.tree, node_id))
# define LOAD_TREE(b_tree) (b_tree.alloc->load(b_tree, b_tree.root))
# define UNLOAD(node_id) (tree.tree.alloc->unload(tree.tree, node_id))
# define UNLOAD_TREE(b_tree) (b_tree.alloc->unload(b_tree, b_tree.root))
# define NEW_NODE() (tree.tree.alloc->new(tree.tree.alloc))
# define FREE(node_id) (tree.tree.alloc->free(tree.tree.alloc, node_id))
# define NOTIFY_DELETED() (tree.tree.alloc->tree_deleted(tree.tree))

/**  Temporary functions to aid in debugging as gdb can't see makros */
/*********************************************************************/
/**/ int16_t numkeys(bt_node *node) {return NUM_KEYS(node);}
/**/ int16_t maxkeys(bt_node *node) {return MAX_KEYS(node);}
/**/ int16_t minkeys(bt_node *node) {return MIN_KEYS(node);}
/**/ uint8_t *pairs(bt_node *node) {return PAIRS(node);}
/**/ uint32_t pair(tree_param tree, bt_node *node, int i)
/**/          {return *(uint32_t*)PAIR(node, i);}
/**/ bt_node_id *children(tree_param tree, bt_node *node) {return CHILDREN(node);}
/**/ bt_node *root(btree_data *tree_data) {return ROOT(tree_data);}
/*********************************************************************/




/*************
 * FUNCTIONS *
 *************/

btree btree_create(bt_alloc_ptr alloc, uint8_t key_size, uint8_t value_size, uint16_t userdata_size){
    bt_node_id tree_node_id = alloc->new(alloc);
    btree tree = (btree){alloc, tree_node_id};
    btree_data *tree_data = LOAD_TREE(tree);
    tree_data->height = -1;
    tree_data->key_size = key_size;
    tree_data->value_size = value_size;
    // Calculate how many keys will fit in each type of node
    // TODO: check correctness, esp. in regards to padding
    tree_data->max_interior_keys = (alloc->node_size-32)
                            / (key_size+value_size+sizeof(bt_node_id)) - 1;
    tree_data->max_leaf_keys = (alloc->node_size-32) / (key_size+value_size) - 1;
    uint16_t max_root_keys = (alloc->node_size-32-sizeof(btree_data)-userdata_size)
                           / (key_size+value_size+sizeof(bt_node_id)) - 1;
    tree_data->root_offset = tree_data->userdata+userdata_size-(char*)tree_data+1;
    // TODO checks that e.g. there is enough space for root
    NUM_KEYS(ROOT(tree_data)) = 0;
    MAX_KEYS(ROOT(tree_data)) = max_root_keys;
    UNLOAD_TREE(tree);
    return tree;
}

void *btree_load_userdata(btree tree){
    btree_data *tree_data = tree.alloc->load(tree, tree.root);
    return tree_data->userdata;
}

void btree_unload_userdata(btree tree){
    tree.alloc->unload(tree, tree.root);
}

// Returns 2*(index of key)+1 if found, even number if between indices
static int search_keys(tree_param tree, bt_node *node, void *key){
    int min = 0;              // min inclusive
    int max = NUM_KEYS(node); // max exclusive
    // binary search
    while(max-min>7){
       int median = (min+max)/2;
       if(memcmp(key, PAIR(node, median), tree.key_size)<0)
           max = median;
       else
           min = median;
    }
    // linear search (could maybe be removed)
    for(;min<max; min++){
        if(memcmp(key, PAIR(node, min), tree.key_size)<0)
            return 2*min;
        if(memcmp(key, PAIR(node, min), tree.key_size)==0)
            return 2*min+1;
    }
    return 2*min;
}

static bt_node *init_node(tree_param tree, bt_node_id node_id, bool leaf){
    bt_node *node = LOAD(node_id);
    btree_data *tree_data = LOAD(tree.tree.root);
    NUM_KEYS(node) = 0;
    MAX_KEYS(node) = leaf ? tree_data->max_leaf_keys:
                            tree_data->max_interior_keys;
    UNLOAD(tree.tree.root);
    return node;
}

// Recursively insert key&value into node. If the node splits, store the id
// of the new node in split_new_node and the seperator between them in split_pair.
// Return true if the key was already present, else false.
static bool insert(tree_param tree, bt_node *node, uint8_t *pair, int height, void *split_pair, bt_node_id *split_new_node_id){
    int index = search_keys(tree, node, pair);
    if(index%2){ // key already present
        memcpy(PAIR(node, index/2), VALUE(pair), tree.pair_size-tree.key_size);
        return true;
    }
    bt_node_id new_node_id = 0;
    int child = index/2;
    if(height){
        bt_node_id child_id = CHILDREN(node)[child];
        bt_node *child_node = LOAD(child_id);
        uint8_t split_pair[tree.pair_size];
        bool present = insert(tree, child_node, pair, height-1, 
                              split_pair, &new_node_id);
        UNLOAD(child_id);
        if(!new_node_id)
            return present;
        pair = split_pair;
    }
    if(NUM_KEYS(node) < MAX_KEYS(node)){
        // enough room, insert new child
        memmove(PAIR(node, child+1), PAIR(node, child), 
                tree.pair_size*(NUM_KEYS(node)-child));
        if(height) // height==0 means leaf → no children
            memmove(CHILD(node, child+2), CHILD(node, child+1), 
                    sizeof(bt_node_id)*(NUM_KEYS(node)-child));
        NUM_KEYS(node)++;
        memcpy(PAIR(node, child), pair, tree.pair_size);
        if(height)
            CHILDREN(node)[child+1] = new_node_id;
        return true;
    } else {
        // Node full
        // TODO: try to push into siblings instead of splitting
        
        // Split node:
        // Initialize new node
        // The max_keys of both nodes will be identical
        bt_node_id right_id = NEW_NODE();
        bt_node *right = init_node(tree, right_id, height==0);
        NUM_KEYS(node) = MIN_KEYS(node) + MAX_KEYS(node)%2; 
        NUM_KEYS(right) = MIN_KEYS(node);

        // If the key is less than the median insert it into the old node,
        // if greater insert into the new one.
        // Copy half of the keys into the right node, lower the left ones num_keys.
        uint8_t median[tree.pair_size];
        if(child == NUM_KEYS(node)){
            // Key in middle
            memcpy(median, pair, tree.pair_size);
            memmove(PAIRS(right), PAIR(node, NUM_KEYS(node)),
                    tree.pair_size*NUM_KEYS(right));
            if(height)
                CHILDREN(right)[0] = new_node_id;
            if(height)
                for(int i = MAX_KEYS(node)+1; i --> NUM_KEYS(node)+1;)
                    CHILDREN(right)[i-NUM_KEYS(node)] = CHILDREN(node)[i];
        } else if(child <= MIN_KEYS(node)){
            // Key in left node
            memmove(PAIRS(right), PAIR(node, NUM_KEYS(node)),
                    tree.pair_size*NUM_KEYS(right));
            if(height)
                for(int i = MAX_KEYS(node)+1; i --> NUM_KEYS(node);)
                    CHILDREN(right)[i-NUM_KEYS(node)] = CHILDREN(node)[i];
            memcpy(median, PAIR(node, NUM_KEYS(node)-1), tree.pair_size);
            memmove(PAIR(node, child+1), PAIR(node, child),
                    tree.pair_size*(NUM_KEYS(node)-child));
            if(height)
                for(int i = NUM_KEYS(node); i --> child+1;)
                    CHILDREN(node)[i+1] = CHILDREN(node)[i];
            memcpy(PAIR(node, child), pair, tree.pair_size);
            if(height)
                CHILDREN(node)[child+1] = new_node_id;
        } else {
            // Key in right node
            memcpy(median, PAIR(node, NUM_KEYS(node)), tree.pair_size);
            memmove(PAIRS(right), PAIR(node, NUM_KEYS(node)+1),
                    tree.pair_size*(child-NUM_KEYS(node)-1));
            if(height)
                for(int i = child+1; i --> NUM_KEYS(node)+1;)
                    CHILDREN(right)[i-NUM_KEYS(node)-1] = CHILDREN(node)[i];
            memcpy(PAIR(right, child-NUM_KEYS(node)-1), pair, tree.pair_size);
            if(height)
                CHILDREN(right)[child-NUM_KEYS(node)] = new_node_id;
            memmove(PAIR(right, child-NUM_KEYS(node)), PAIR(node, child),
                    tree.pair_size*(MAX_KEYS(node)-child));
            if(height)
                for(int i = MAX_KEYS(node)+1; i --> child+1;)
                    CHILDREN(right)[i-NUM_KEYS(node)] = CHILDREN(node)[i];
        }
        //TODO: eliminate this memcpy
        memcpy(split_pair, median, tree.pair_size);
        *split_new_node_id = right_id;   //Unload?
        return false;
    }
}

bool btree_insert(btree b_tree, void *key, void *value){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = {b_tree, tree_data->key_size, 
                           tree_data->key_size+tree_data->value_size};
    bt_node *root = ROOT(tree_data);
    uint8_t pair[tree.pair_size];
    memcpy(pair, key, tree.key_size);
    memcpy(pair+tree.pair_size, value, tree.pair_size-tree.key_size);
    if(tree_data->height==-1){
        // Tree is empty
        tree_data->height = 0;
        NUM_KEYS(root) = 1;
        memcpy(PAIR(root, 0), pair, tree.pair_size);

        UNLOAD_TREE(b_tree);
        return false;
    } else {
        uint8_t split_pair[tree.pair_size];
        bt_node_id split_id = 0;
        bool already_present = insert(tree, root,
                pair, tree_data->height, split_pair, &split_id);
        if(split_id){
            bt_node *new_node = LOAD(split_id);
            
            // Root node may be smaller than others, in which case we can't
            // split it (resulting nodes would be below their min_keys).
            if(MAX_KEYS(root)<MAX_KEYS(new_node)){
                // In that case move root node data into the new node
				// and make that a child of the root (root will have 0 keys).
				memmove(PAIR(new_node, NUM_KEYS(root)+1), PAIRS(new_node),
						NUM_KEYS(new_node)*tree.pair_size);
				memcpy(PAIR(new_node, NUM_KEYS(root)), split_pair, tree.pair_size);
				memmove(PAIRS(new_node), PAIRS(root),
						NUM_KEYS(root)*tree.pair_size);
				if(tree_data->height){
					for(int i=NUM_KEYS(new_node)+1; i --> 0;)
						CHILDREN(new_node)[i+NUM_KEYS(root)+1]
							= CHILDREN(new_node)[i];
					for(int i=NUM_KEYS(root)+1; i --> 0;)
						CHILDREN(new_node)[i] = CHILDREN(root)[i];
				}
				
				NUM_KEYS(new_node) += NUM_KEYS(root)+1;
				NUM_KEYS(root) = 0;
				CHILDREN(root)[0] = split_id;
			} else {
                // If that is not the case, move the previous root out
                // and store both nodes in the new root
                bt_node_id new_left_id = NEW_NODE();
                bt_node *new_left = LOAD(new_left_id);
                NUM_KEYS(new_left) = NUM_KEYS(root);
                MAX_KEYS(new_left) = MAX_KEYS(root);
                
				memmove(PAIRS(new_left), PAIRS(root), NUM_KEYS(new_left)*tree.pair_size);
                // If execution reaches here, root is interior
                for(int i=NUM_KEYS(new_left)+1; i --> 0;)
                    CHILDREN(new_left)[i] = CHILDREN(root)[i];

                UNLOAD(new_left_id);
                
                NUM_KEYS(root) = 1;
                memcpy(PAIR(root, 0), split_pair, tree.pair_size);
                CHILDREN(root)[0] = new_left_id;
                CHILDREN(root)[1] = split_id;
			}

            UNLOAD(split_id);
            tree_data->height++;
        }
        UNLOAD_TREE(b_tree);
        return already_present;
    }
}



static bool search(tree_param tree, bt_node* node, void *key, uint8_t height, void *value_writeback){
    int index = search_keys(tree, node, key);
    if(index%2==1) {
        // key is in pairs
        if(value_writeback)
            memcpy(value_writeback, PAIR(node, index/2), tree.pair_size);
        return true;
    } if(height==0)
        // leaf node & key's not a child
        return false;
    else {
        // recurse
        bt_node_id child_id = CHILDREN(node)[index/2];
        bt_node *child = LOAD(child_id);
        return search(tree, child, key, height-1, value_writeback);
        UNLOAD(child_id);
    }
}

bool btree_contains(btree b_tree, void *key){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = (tree_param){b_tree, tree_data->key_size, 
                        tree_data->key_size + tree_data->value_size};
    if(tree_data->height>=0){
        UNLOAD_TREE(b_tree);
        return search(tree, ROOT(tree_data), key, tree_data->height, NULL);
    } else {
        UNLOAD_TREE(b_tree);
        return false;
    }
}

void btree_get(btree b_tree, void *key, void *value, bool *success){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = (tree_param){b_tree, tree_data->key_size, 
                        tree_data->key_size + tree_data->value_size};
    if(tree_data->height>=0){
        bool found = search(tree, ROOT(tree_data), key, tree_data->height, value);
        if(success)
            *success = found;
    } else if(success) {
        *success = false;
    }
    UNLOAD_TREE(b_tree);
}



static bool traverse(tree_param tree, bt_node *node,
        bool (*callback)(void*, void*, void*),
        void* params, bool reverse, int height){
    if(!reverse)
        for(int i=0; i <= NUM_KEYS(node); i++){
            if(height) {
                bt_node_id child_id = CHILDREN(node)[i];
                bt_node *child = LOAD(child_id);
                traverse(tree, child, callback, params, reverse, height-1);
                UNLOAD(child_id);
            }
            if(i<NUM_KEYS(node))
                if(callback(PAIR(node, i), VALUE(PAIR(node, i)), params))
                    return true;
        }
    else
        for(int i=NUM_KEYS(node)+1; i --> 0;){
            if(height) {
                bt_node_id child_id = CHILDREN(node)[i];
                bt_node *child = LOAD(child_id);
                traverse(tree, child, callback, params, reverse, height-1);
                UNLOAD(child_id);
            }
            if(i<NUM_KEYS(node))
                if(callback(PAIR(node, i), VALUE(PAIR(node, i)), params))
                    return true;
        }
    return false;
}

bool btree_traverse(btree b_tree, 
        bool (*callback)(void*, void*, void*),
        void* id, bool reverse){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = (tree_param){b_tree, tree_data->key_size, 
                        tree_data->key_size + tree_data->value_size};
    bool aborted = false;
    if(tree_data->height>=0)
        aborted = traverse(tree, ROOT(tree_data), callback, 
                           id, reverse, tree_data->height);
    UNLOAD_TREE(b_tree);
    return aborted;
}

static void find_smallest(tree_param tree, bt_node *node, int height, void *writeback){
    if(!height)
        memcpy(writeback, PAIR(node, 0), tree.pair_size);
    else {
        bt_node_id child_id = CHILDREN(node)[0];
        bt_node *child = LOAD(child_id);
        find_smallest(tree, child, height-1, writeback);
        UNLOAD(child_id);
    }
}

static void find_biggest(tree_param tree, bt_node *node, int height, void *writeback){
    if(!height)
        memcpy(writeback, PAIR(node, NUM_KEYS(node)-1), tree.pair_size);
    else {
        bt_node_id child_id = CHILDREN(node)[0];
        bt_node *child = LOAD(child_id);
        find_biggest(tree, child, height-1, writeback);
        UNLOAD(child_id);
    }
}

static void free_node(tree_param tree, bt_node *node, int height){
    if(height>0)
        for(int i=NUM_KEYS(node)+1; i --> 0;){
            bt_node_id child_id = CHILDREN(node)[i];
            if(height>1) {
                bt_node *child = LOAD(child_id);
                free_node(tree, child, height-1);
            }
            // free may be called even if node is loaded
            FREE(child_id);
        }
}

void btree_delete(btree b_tree){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = (tree_param){b_tree, tree_data->key_size, 
                        tree_data->key_size + tree_data->value_size};
    if(tree_data->height>=0)
        free_node(tree, ROOT(tree_data), tree_data->height);
    FREE(b_tree.root);
    NOTIFY_DELETED();
}

static bool remove_key(tree_param tree, bt_node *node, void *key, int height){
    int index = search_keys(tree, node, key);
    if(!height){
        if(!(index%2))
            return false;
        memmove(PAIR(node, index/2), PAIR(node, index/2+1), 
                (NUM_KEYS(node)-1-index/2)*tree.pair_size);
        // parent will check if below min number of keys
        NUM_KEYS(node)--;
        return true;
    } else {
        int child_index = index/2;
        bt_node_id child_id;
        bt_node *cn;
        bool found;
        if(!(index%2)){
            // remove key from child
            child_id = CHILDREN(node)[child_index];
            cn = LOAD(child_id);
            found = remove_key(tree, cn, key, height-1);
        } else if(child_index<NUM_KEYS(node)){
            // the smallest key in the right subtree works as seperator
            child_index++;
            child_id = CHILDREN(node)[child_index];
            cn = LOAD(child_id);
            find_smallest(tree, cn, height-1, PAIR(node, index/2));
            remove_key(tree, cn, PAIR(node, index/2), height-1);
            found = true;
        } else {
            // the biggest key in the left subtree works as seperator
            child_id = CHILDREN(node)[child_index];
            cn = LOAD(child_id);
            find_biggest(tree, cn, height-1, PAIR(node, index/2));
            remove_key(tree, cn, PAIR(node, index/2), height-1);
            found = true;
        }
        // rebalance if child below min number of keys
        if(NUM_KEYS(cn)<MIN_KEYS(cn)){
            // check immediate siblings for available key
            // take from left if possible
            bt_node_id prev_id = CHILDREN(node)[child_index-1];
            bt_node *prev = LOAD(prev_id);
            if(child_index>0 && NUM_KEYS(prev)>MIN_KEYS(prev)){
                memmove(PAIR(cn, 1), PAIRS(cn), NUM_KEYS(cn)*tree.pair_size);
                if(height-1)
                    for(int i = NUM_KEYS(cn)+1; i --> 0;)
                        CHILDREN(cn)[i+1] = CHILDREN(cn)[i];
                memcpy(PAIR(cn, 0), PAIR(node, child_index-1), tree.pair_size);
                memcpy(PAIR(node, child_index-1), PAIR(prev, NUM_KEYS(prev)-1),
                        tree.pair_size);
                if(height-1)
                    CHILDREN(cn)[0] = CHILDREN(prev)[NUM_KEYS(prev)];
                NUM_KEYS(prev)--;
                NUM_KEYS(cn)++;
            } else {
                bt_node_id next_id = CHILDREN(node)[child_index+1];
                bt_node *next = LOAD(next_id);

                // else take from right if possible
                if(child_index<NUM_KEYS(node) && NUM_KEYS(next)>MIN_KEYS(next)){
                    memcpy(PAIR(cn, NUM_KEYS(cn)), PAIR(node, child_index), 
                           tree.pair_size);
                    memcpy(PAIR(node, child_index), PAIR(next, 0), tree.pair_size);
                    memmove(PAIRS(next), PAIR(next, 1), NUM_KEYS(next)*tree.pair_size);
                    if(height-1){
                        CHILDREN(cn)[NUM_KEYS(cn)+1] = CHILDREN(next)[0];
                        for(int i = 0; i < NUM_KEYS(next); i++)
                            CHILDREN(next)[i] = CHILDREN(next)[i+1];
                    }
                    NUM_KEYS(next)--;
                    NUM_KEYS(cn)++;
                    UNLOAD(next_id);
                } else {
                    // If none available in siblings, merge
                    
                    // Make sure it works both when child is leftmost and rightmost
                    bt_node *left, *right;
                    int left_index;
                    if(child_index == 0){
                        left = cn;
                        right = next;
                        left_index = 0;
                    } else {
                        left = prev;
                        right = cn;
                        left_index = child_index - 1;
                    }
                    
                    // Merge into left
                    memcpy(PAIR(left, NUM_KEYS(left)), PAIR(node, left_index),
                           tree.pair_size);
                    memmove(PAIR(node, left_index), PAIR(node, left_index+1),
                            NUM_KEYS(node)*tree.pair_size);
                    for(int i = left_index+1; i < NUM_KEYS(node); i++)
                        CHILDREN(node)[i] = CHILDREN(node)[i+1];
                    memmove(PAIR(left, NUM_KEYS(left)+1), PAIRS(right),
                            NUM_KEYS(right)*tree.pair_size);
                    if(height-1)
                        for(int i = NUM_KEYS(right)+1; i --> 0;) 
                            CHILDREN(left)[i+NUM_KEYS(left)+1] = CHILDREN(right)[i];
                    NUM_KEYS(left) += 1 + NUM_KEYS(right);
                    NUM_KEYS(node)--;
                    
                    // Free right, mark as to not trigger an unload after free
                    if(child_index){
                        FREE(child_id);
                        child_id = 0;
                    } else {
                        FREE(next_id);
                        next_id = 0;
                    }
                }
                // only unload if not already freed
                if(next_id)
                    UNLOAD(next_id);
            }
            UNLOAD(prev_id);
        }
        // only unload if not already freed
        if(child_id)
            UNLOAD(child_id);
        return found;
    }
}

bool btree_remove(btree b_tree, void *key){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = (tree_param){b_tree, tree_data->key_size, 
                        tree_data->key_size + tree_data->value_size};
    if(tree_data->height>=0){
        bt_node *root = ROOT(tree_data);
        bool found;
        // Root may have fewer than min_keys keys.
        // If it has zero keys, it contains only the id of the actual root
        // In that case we have to remove_key() from that instead
        // as a sibling is required for merging.
        if(NUM_KEYS(root)==0){
            bt_node_id proxied_root_id = CHILDREN(root)[0];
            bt_node *proxied_root = LOAD(proxied_root_id);
            found = remove_key(tree, proxied_root, key, tree_data->height-1);
            
            // If the actual root now fits into the tree root again,
            // its data can be moved there
            if(NUM_KEYS(proxied_root)==MAX_KEYS(root)){
                NUM_KEYS(root) = MAX_KEYS(root);
                memmove(PAIRS(root), PAIRS(proxied_root), NUM_KEYS(root)*tree.pair_size);
                if(tree_data->height > 1)
                    for(int i=NUM_KEYS(root)+1; i --> 0;)
                        CHILDREN(root)[i] = CHILDREN(proxied_root)[i];
                FREE(proxied_root_id);
                tree_data->height--;
            } else {
                UNLOAD(proxied_root_id);
            }
        } else {
            found = remove_key(tree, root, key, tree_data->height); 
        }
        
        // Check if tree is empty
        if((NUM_KEYS(root)==0 && tree_data->height==0) || NUM_KEYS(root)==-1){
            tree_data->height = -1;
        }

        UNLOAD_TREE(b_tree);
        return found;
    } else {
        UNLOAD_TREE(b_tree);
        return false;
    }
}



// Recursove function, to be called only by btree_debug_print() (and itself).
// Height is the distance to the leafs, max_height is the height of the root,
// startc is a graph line connection character (unicode), lines_above and _below
// are bitmask for whether to draw vertical lines at the given height
static void debug_print(tree_param tree, FILE *stream, bt_node* node, bool print_value, int height, int max_height, char *startc, uint32_t lines_above, uint32_t lines_below){
    for(int i = 0; i < NUM_KEYS(node)+1; i++){
        // Print child
        if(height){
            bt_node_id child_id = CHILDREN(node)[i];
            bt_node *child = LOAD(child_id);
            uint32_t lines_row = i<(NUM_KEYS(node)+1)/2?lines_above:lines_below;
            debug_print(tree, stream, child, print_value, height-1, max_height,
                    i==0 ?              "╭"
                  : i==NUM_KEYS(node) ? "╰"
                  :                     "├",
                  lines_row|(i==0?0:1<<(max_height-height)),
                  lines_row|(i==NUM_KEYS(node)?0:1<<(max_height-height)));
            UNLOAD(child_id);
        }
        // Print key
        if(i<NUM_KEYS(node)){
            // Print space and horizontal lines
            for(int s = 0; s<(max_height-height)*6; s++)
                fputs(s%6==5 && (i<NUM_KEYS(node)/2?lines_above:lines_below)&1<<(s/6) ?"│":" ", stream);
            // Print horizontal lines & connectors
            if(!height){ // leaf nodes
                if(i==0 && NUM_KEYS(node)==1)
                    fprintf(stream, "\033[D%s──────", startc);
                else if(i==0 && NUM_KEYS(node)==2)
                    fprintf(stream, "\033[D%s─────┬", startc);
                else if(i==0)
                    fprintf(stream, "     ╭");
                else if(i==(NUM_KEYS(node)-1)/2)
                    fprintf(stream, "\033[D%s─────┤", startc);
                else if(i==NUM_KEYS(node)-1)
                    fprintf(stream, "     ╰");
                else
                    fprintf(stream, "     │");
            } else { // interior nodes
                if(i==(NUM_KEYS(node)-1)/2)
                    fprintf(stream, "\033[D%s─────┼", startc);
                else
                    fprintf(stream, "     ├");
            }
            // Print key & value
            for(int j = 0; j < tree.key_size; j++)
                fprintf(stream, "%02x", *((uint8_t*)PAIR(node, i)+j));
            if(print_value){
                fputs(" → ", stream);
                for(int j = 0; j < tree.key_size; j++)
                    fprintf(stream, "%02x", *((uint8_t*)PAIR(node, i)+j));
            }
            fputs("\n", stream);
        }
    }
}

void btree_debug_print(FILE *stream, btree b_tree, bool print_value){
    btree_data *tree_data = LOAD_TREE(b_tree);
    tree_param tree = (tree_param){b_tree, tree_data->key_size, 
                        tree_data->key_size + tree_data->value_size};
    if(tree_data->height >= 0){
        bt_node *root = ROOT(tree_data);
        if(NUM_KEYS(root)==0){
            bt_node_id proxied_root_id = CHILDREN(root)[0];
            bt_node *proxied_root = LOAD(proxied_root_id);
            debug_print(tree, stream, proxied_root, print_value,
                    tree_data->height-1, tree_data->height-1, "", 0, 0);
            UNLOAD(proxied_root_id);
        } else {
            debug_print(tree, stream, root, print_value,
                    tree_data->height, tree_data->height, "", 0, 0);
        }
    }  else {
        puts("(empty)");
    }
    UNLOAD_TREE(b_tree);
}
