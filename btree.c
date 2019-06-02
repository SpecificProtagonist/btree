#include <stdlib.h>
#include <stdio.h>
#include "btree.h"

/*************
 * DATATYPES *
 *************/

// Represents a keys-value pair
typedef struct {
    bt_key key;
    bt_value value;
} bt_pair;

typedef void bt_node;

// Inserting/deleting keys/values/children is O(MAX_KEYS),
// so maybe use binary tree in array representation instead of array?
// Would be more complex though.
// TODO: worry about alignment
// TODO: Allow variable datatype sizes (including bt_node_id)
// Currently this could cause problems with alignment, which will be fixed
// if datatypes are represented as variable length char arrays.
/// Node structure
/*  uint16_t   num_keys
 *  uint16_t   max_keys
 *  bt_pair    pairs[max_keys]
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
    // Custom data (variable length) stored alongside tree
    char userdata[1];
} btree_data;




/**********
 * MAKROS *
 **********/

// Accessor makros since nodes aren't structs
# define NUM_KEYS(node) (*(int16_t*)node)
# define MAX_KEYS(node) (*((int16_t*)node+1))
# define MIN_KEYS(node) (MAX_KEYS(node)/2)
# define PAIRS(node)    ((bt_pair*)((int16_t*)node+2))
# define CHILDREN(node) ((bt_node_id*)(PAIRS(node)+MAX_KEYS(node)))

# define ROOT(tree_data) ((bt_node*)((char*)tree_data+tree_data->root_offset))

// Common operations for brevity
// These require the variable tree of type btree.
# define LOAD(node_id) (tree.alloc->load(tree, node_id))
# define UNLOAD(node_id) (tree.alloc->unload(tree, node_id))
# define NEW_NODE() (tree.alloc->new(tree.alloc))
# define FREE(node_id) (tree.alloc->free(tree.alloc, node_id))
# define NOTIFY_DELETED() (tree.alloc->tree_deleted(tree))




/*************
 * FUNCTIONS *
 *************/

btree btree_create(bt_alloc_ptr alloc, uint16_t userdata_size){
    bt_node_id tree_node = alloc->new(alloc);
    btree tree = (btree){alloc, tree_node};
    btree_data *tree_data = alloc->load(tree, tree_node);
    tree_data->height = 0;
    // Calculate how many keys will fit in each type of node
    // TODO: check correctness, esp. in regards to padding
    tree_data->max_interior_keys = (alloc->node_size-32)
                            / (sizeof(struct{bt_pair a; void* b;})) - 1;
    tree_data->max_leaf_keys = (alloc->node_size-32) / sizeof(bt_pair) - 1;
    uint16_t max_root_keys = (alloc->node_size-32-userdata_size)
                           / (sizeof(struct{bt_pair a; void* b;})) - 1;
    // TODO checks that e.g. there is enough space for root
    // TODO root node in same page (→ also tree->height==-1 instead of !tree->root)
    // maybe also store allocator-specific data
    alloc->unload(tree, tree_node);
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
// (therefore can't use bsearch())
static int search_keys(bt_node *node, bt_key key){
    int min = 0;              // min inclusive
    int max = NUM_KEYS(node); // max exclusive
    // binary search
    while(max-min>7){
       int median = (min+max)/2;
       if(key < PAIRS(node)[median].key)
           max = median;
       else
           min = median;
    }
    // linear search (could maybe be removed)
    for(;min<max; min++){
        if(key < PAIRS(node)[min].key)
            return 2*min;
        if(key == PAIRS(node)[min].key)
            return 2*min+1;
    }
    return 2*min;
}

static bt_node *init_node(btree tree, bt_node_id node_id, bool leaf){
    bt_node *node = LOAD(node_id);
    btree_data *tree_data = LOAD(tree.root);
    NUM_KEYS(node) = 0;
    MAX_KEYS(node) = leaf ? tree_data->max_leaf_keys:
                            tree_data->max_interior_keys;
    UNLOAD(tree.root);
    return node;
}

typedef struct {
    bool already_present;
    bt_pair pair;
    bt_node_id new_node_id;
} insert_result;

static insert_result insert(btree tree, bt_node *node, bt_pair pair, int height){
    int index = search_keys(node, pair.key);
    if(index%2){ // key already present
        PAIRS(node)[index/2].value = pair.value;
        return (insert_result){true};
    }
    bt_node_id new_node;
    int child = index/2;
    if(height){
        bt_node_id child_id = CHILDREN(node)[child];
        bt_node *child_node = LOAD(child_id);
        insert_result split = 
            insert(tree, child_node, pair, height-1);
        UNLOAD(child_id);
        if(!split.new_node_id)
            return split;
        pair = split.pair;
        new_node = split.new_node_id;
    }
    if(NUM_KEYS(node) < MAX_KEYS(node)){
        // enough room, insert new child
        for(int i = NUM_KEYS(node); i --> child;)
            PAIRS(node)[i+1] = PAIRS(node)[i];
        if(height) // height==0 means leave → no children
            for(int i = NUM_KEYS(node); i > child; i--)
                CHILDREN(node)[i+1] = CHILDREN(node)[i];
        NUM_KEYS(node)++;
        PAIRS(node)[child] = pair;
        if(height)
            CHILDREN(node)[child+1] = new_node;
        return (insert_result){true};
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
        bt_pair median;
        if(child == NUM_KEYS(node)){
            // Key in middle
            median = pair;
            for(int i = MAX_KEYS(node); i --> NUM_KEYS(node);)
                PAIRS(right)[i-NUM_KEYS(node)] = PAIRS(node)[i];
            if(height)
                CHILDREN(right)[0] = new_node;
            if(height)
                for(int i = MAX_KEYS(node)+1; i --> NUM_KEYS(node)+1;)
                    CHILDREN(right)[i-NUM_KEYS(node)] = CHILDREN(node)[i];
        } else if(child <= MIN_KEYS(node)){
            // Key in left node
            for(int i = MAX_KEYS(node); i --> NUM_KEYS(node);)
                PAIRS(right)[i-NUM_KEYS(node)] = PAIRS(node)[i];
            if(height)
                for(int i = MAX_KEYS(node)+1; i --> NUM_KEYS(node);)
                    CHILDREN(right)[i-NUM_KEYS(node)] = CHILDREN(node)[i];
            median = PAIRS(node)[NUM_KEYS(node)-1];
            for(int i = NUM_KEYS(node)-1; i --> child;)
                PAIRS(node)[i+1] = PAIRS(node)[i];
            if(height)
                for(int i = NUM_KEYS(node); i --> child+1;)
                    CHILDREN(node)[i+1] = CHILDREN(node)[i];
            PAIRS(node)[child] = pair;
            if(height)
                CHILDREN(node)[child+1] = new_node;
        } else {
            // Key in right node
            median = PAIRS(node)[NUM_KEYS(node)];
            for(int i = child; i --> NUM_KEYS(node)+1;)
                PAIRS(right)[i-NUM_KEYS(node)-1] = PAIRS(node)[i];
            if(height)
                for(int i = child+1; i --> NUM_KEYS(node)+1;)
                    CHILDREN(right)[i-NUM_KEYS(node)-1] = CHILDREN(node)[i];
            PAIRS(right)[child-NUM_KEYS(node)-1] = pair;
            if(height)
                CHILDREN(right)[child-NUM_KEYS(node)] = new_node;
            for(int i = MAX_KEYS(node); i --> child;)
                PAIRS(right)[i-NUM_KEYS(node)] = PAIRS(node)[i];
            if(height)
                for(int i = MAX_KEYS(node)+1; i --> child+1;)
                    CHILDREN(right)[i-NUM_KEYS(node)] = CHILDREN(node)[i];
        }
        return (insert_result){false, median, right_id};
    }
}

bool btree_insert(btree tree, bt_key key, bt_value value){
    btree_data *tree_data = LOAD(tree.root);
    bt_node *root = ROOT(tree_data);
    if(tree_data->height==-1){
        // Tree is empty
        tree_data->height = 0;
        NUM_KEYS(root) = 1;
        PAIRS(root)[0] = (bt_pair){.key=key, .value=value};
        UNLOAD(tree.root);
        return false;
    } else {
        insert_result split = insert(tree, root,
                (bt_pair){.key=key, .value=value}, tree_data->height);
        if(split.new_node_id){
            bt_node *new_node = LOAD(split.new_node_id);
            // Root node is smaller than others, therefore we can't
            // split it (resulting nodes would be below their min_keys).
            // Instead, move root node data into the new node
            // and make that a child of the root (root will have 0 keys).
            for(int i=NUM_KEYS(new_node); i --> 0;)
                PAIRS(split.new_node_id)[i+NUM_KEYS(root)+1]
                    = PAIRS(new_node)[i];
            PAIRS(new_node)[NUM_KEYS(root)] = split.pair;
            for(int i=NUM_KEYS(root); i --> 0;)
                PAIRS(new_node)[i] = PAIRS(root)[i];
            if(tree_data->height){
                for(int i=NUM_KEYS(new_node)+1; i --> 0;)
                    CHILDREN(new_node)[i+NUM_KEYS(root)+1]
                        = CHILDREN(new_node)[i];
                for(int i=NUM_KEYS(root)+1; i --> 0;)
                    CHILDREN(new_node)[i] = CHILDREN(root)[i];
            }

            NUM_KEYS(root) = 0;
            CHILDREN(root)[0] = split.new_node_id;

            UNLOAD(split.new_node_id);
            tree_data->height++;
        }
        UNLOAD(tree.root);
        return split.already_present;
    }
}



typedef struct {
    bt_value value;
    bool found;
} search_result;

static search_result search(btree tree, bt_node* node, bt_key key, uint8_t height){
    int index = search_keys(node, key);
    if(index%2==1)
        // key in PAIRS(node)
        return (search_result){PAIRS(node)[index/2].value, true};
    if(height==0)
        // leaf node & key's not a child
        return (search_result){.found=false};
    else {
        // recurse
        bt_node_id child_id = CHILDREN(node)[index/2];
        bt_node *child = LOAD(child_id);
        return search(tree, child, key, height-1);
        UNLOAD(child_id);
    }
}

bool btree_contains(btree tree, bt_key key){
    btree_data *tree_data = LOAD(tree.root);
    if(tree_data->height>=0){
        UNLOAD(tree.root);
        return search(tree, ROOT(tree_data), key, tree_data->height).found;
    } else {
        UNLOAD(tree.root);
        return false;
    }
}

bt_value btree_get(btree tree, bt_key key, bool *success){
    btree_data *tree_data = LOAD(tree.root);
    if(tree_data->height>=0){
        search_result result = search(tree, ROOT(tree_data), key, tree_data->height);
        *success = result.found;
        UNLOAD(tree.root);
        return result.value;
    } else {
        *success = false;
        UNLOAD(tree.root);
        return (bt_value){0};
    }
}

bt_value btree_get_or_default(btree tree, bt_key key, bt_value alt){
    bool success;
    bt_value value = btree_get(tree, key, &success);
    return success ? value : alt;
}



static void traverse(btree tree, bt_node *node,
        bt_value(*callback)(bt_key, bt_value, void*),
        void* id, bool reverse, int height){
    if(!reverse)
        for(int i=0; i <= NUM_KEYS(node); i++){
            if(height) {
                bt_node_id child_id = CHILDREN(node)[i];
                bt_node *child = LOAD(child_id);
                traverse(tree, child, callback, id, reverse, height-1);
                UNLOAD(child_id);
            }
            if(i<NUM_KEYS(node))
                PAIRS(node)[i].value = 
                    callback(PAIRS(node)[i].key, PAIRS(node)[i].value, id);
        }
    else
        for(int i=NUM_KEYS(node)+1; i --> 0;){
            if(height) {
                bt_node_id child_id = CHILDREN(node)[i];
                bt_node *child = LOAD(child_id);
                traverse(tree, child, callback, id, reverse, height-1);
                UNLOAD(child_id);
            }
            if(i<NUM_KEYS(node))
                PAIRS(node)[i].value = 
                    callback(PAIRS(node)[i].key, PAIRS(node)[i].value, id);
        }
}

void btree_traverse(btree tree, 
        bt_value(*callback)(bt_key, bt_value, void*),
        void* id, bool reverse){
    btree_data *tree_data = LOAD(tree.root);
    if(tree_data->height>=0)
        traverse(tree, ROOT(tree_data), callback, id, reverse, tree_data->height);
    UNLOAD(tree.root);
}

static bt_pair find_smallest(btree tree, bt_node *node, int height){
    if(!height)
        return PAIRS(node)[0];
    else {
        bt_node_id child_id = CHILDREN(node)[0];
        bt_node *child = LOAD(child_id);
        bt_pair smallest = find_smallest(tree, child, height-1);
        UNLOAD(child_id);
        return smallest;
    }
}

static bt_pair find_biggest(btree tree, bt_node *node, int height){
    if(!height)
        return PAIRS(node)[NUM_KEYS(node)-1];
    else {
        bt_node_id child_id = CHILDREN(node)[0];
        bt_node *child = LOAD(child_id);
        bt_pair biggest = find_biggest(tree, child, height-1);
        UNLOAD(child_id);
        return biggest;
    }
}

static void free_node(btree tree, bt_node *node, int height){
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

void btree_delete(btree tree){
    btree_data *tree_data = LOAD(tree.root);
    if(tree_data->height>=0)
        free_node(tree, ROOT(tree_data), tree_data->height);
    FREE(tree.root);
    NOTIFY_DELETED();
}

static bool remove_key(btree tree, bt_node *node, bt_key key, int height){
    int index = search_keys(node, key);
    if(!height){
        if(!(index%2))
            return false;
        for(int i = index/2; i < NUM_KEYS(node)-1; i++)
            PAIRS(node)[i] = PAIRS(node)[i+1];
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
            bt_pair seperator = find_smallest(tree, cn, height-1);
            PAIRS(node)[index/2] = seperator;
            remove_key(tree, cn, seperator.key, height-1);
            found = true;
        } else {
            // the biggest key in the left subtree works as seperator
            child_id = CHILDREN(node)[child_index];
            cn = LOAD(child_id);
            bt_pair seperator = find_biggest(tree, cn, height-1);
            PAIRS(node)[index/2] = seperator;
            remove_key(tree, cn, seperator.key, height-1);
            found = true;
        }
        // rebalance if child below min number of keys
        if(NUM_KEYS(cn)<MIN_KEYS(cn)){
            // check immediate siblings for available key
            // take from left if possible
            bt_node_id prev_id = CHILDREN(node)[child_index-1];
            bt_node *prev = LOAD(prev_id);
            if(child_index>0 && NUM_KEYS(prev)>MIN_KEYS(prev)){
                for(int i = NUM_KEYS(cn); i --> 0;)
                    PAIRS(cn)[i+1] = PAIRS(cn)[i];
                if(height-1)
                    for(int i = NUM_KEYS(cn)+1; i --> 0;)
                        CHILDREN(cn)[i+1] = CHILDREN(cn)[i];
                PAIRS(cn)[0] = PAIRS(node)[child_index-1];
                PAIRS(node)[child_index-1] = PAIRS(prev)[NUM_KEYS(prev)-1];
                if(height-1)
                    CHILDREN(cn)[0] = CHILDREN(prev)[NUM_KEYS(prev)];
                NUM_KEYS(prev)--;
                NUM_KEYS(cn)++;
            } else {
                bt_node_id next_id = CHILDREN(node)[child_index+1];
                bt_node *next = LOAD(next_id);

                // else take from right if possible
                if(child_index<NUM_KEYS(node) && NUM_KEYS(next)>MIN_KEYS(next)){
                    PAIRS(cn)[NUM_KEYS(cn)] = PAIRS(node)[child_index];
                    PAIRS(node)[child_index] = PAIRS(next)[0];
                    for(int i = 0; i < NUM_KEYS(next)-1; i++)
                        PAIRS(next)[i] = PAIRS(next)[i+1];
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
                    PAIRS(left)[NUM_KEYS(left)] = PAIRS(node)[left_index];
                    for(int i = left_index; i < NUM_KEYS(node)-1; i++)
                        PAIRS(node)[i] = PAIRS(node)[i+1];
                    for(int i = left_index+1; i < NUM_KEYS(node); i++)
                        CHILDREN(node)[i] = CHILDREN(node)[i+1];
                    for(int i = NUM_KEYS(right); i --> 0;)
                        PAIRS(left)[i+NUM_KEYS(left)+1] = PAIRS(right)[i];
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

bool btree_remove(btree tree, bt_key key){
    btree_data *tree_data = LOAD(tree.root);
    if(tree_data->height>=0){
        bt_node *root = ROOT(tree_data);
        bool found;
        // Root may have fewer than min_keys keys.
        // If it has zero keys, it contains only the id of the actual root
        // In that case we have to remove_key() from that instead
        // as a sibling is required for merging.
        if(NUM_KEYS(root)==0){
            bt_node_id actual_root_id = CHILDREN(root)[0];
            bt_node *actual_root = LOAD(actual_root_id);
            found = remove_key(tree, actual_root, key, tree_data->height);
            
            // If the actual root now fits into the tree root again,
            // its data can be moved there
            if(NUM_KEYS(actual_root)==MAX_KEYS(root)){
                NUM_KEYS(root) = MAX_KEYS(root);
                for(int i=NUM_KEYS(root); i --> 0;)
                    PAIRS(root)[i] = PAIRS(actual_root)[i];
                if(tree_data->height > 1)
                    for(int i=NUM_KEYS(root)+1; i --> 0;)
                        CHILDREN(root)[i] = CHILDREN(actual_root)[i];
                FREE(actual_root_id);
            } else {
                UNLOAD(actual_root_id);
            }
        } else {
            found = remove_key(tree, root, key, tree_data->height); 
        }
        
        // Check if tree is empty
        if((NUM_KEYS(root)==0 && tree_data->height==0) || NUM_KEYS(root)==-1){
            tree_data->height = -1;
        }

        UNLOAD(tree.root);
        return found;
    } else {
        UNLOAD(tree.root);
        return false;
    }
}



// Recursove function, to be called only by btree_debug_print() (and itself).
// Height is the distance to the leafs, max_height is the height of the root,
// startc is a graph line connection character (unicode), lines_above and _below
// are bitmask for whether to draw vertical lines at the given height
static void debug_print(btree tree, FILE *stream, bt_node* node, bool print_value, int height, int max_height, char *startc, uint32_t lines_above, uint32_t lines_below){
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
            if(print_value)
                // Kinda hacky, but I don't think there's a better way
                #define BT_PRINT_FORMAT _Generic(((bt_value){0}), \
                    uint32_t: "%x → %x\n", uint64_t: "%x → %llx\n", \
                    int32_t:  "%x → %x\n", int64_t:  "%x → %llx\n", \
                    char*:    "%x → %s\n", default:  "%x → %p\n")
                fprintf(stream, BT_PRINT_FORMAT,
                        PAIRS(node)[i].key, PAIRS(node)[i].value);
            else
                fprintf(stream, "%x\n", PAIRS(node)[i].key);
        }
    }
}

void btree_debug_print(FILE *stream, btree tree, bool print_value){
    btree_data *tree_data = LOAD(tree.root);
    if(tree_data->height >= 0)
        debug_print(tree, stream, ROOT(tree_data), print_value, tree_data->height, tree_data->height, "", 0, 0);
    else
        puts("(empty)");
    UNLOAD(tree.root);
}
