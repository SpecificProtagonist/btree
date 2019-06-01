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

typedef uint32_t offset;

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

// Accessor makros since nodes aren't structs
# define NUM_KEYS(node) *((uint16_t)node)
# define MAX_KEYS(node) *(((uint16_t*)node)+1)
# define MIN_KEYS(node) (MAX_KEYS(node)/2)
# define PAIRS(node)    (bt_pair)((uint32_t)+2)
# define CHILDREN(node) (bt_node_id)(PAIRS(node)+MAX_KEYS(NODE))

// Tree metadata, kept in a node
typedef struct {
    // Root node of the tree, will be located on same page/node
    void *root;
    // Allocator used for this tree
    bt_alloc_ptr alloc;
    // Maximum number of keys for each key type (will differ for root)
    uint16_t max_interior_keys;
    uint16_t max_leaf_keys;
    // Height of the tree. -1 Means empty tree, 0 means the root is a leaf.
    int8_t height;
} btree_data;




/*************
 * FUNCTIONS *
 *************/


btree btree_create(bt_alloc_ptr alloc, uint16_t userdata_size){
    bt_node_id tree_node = alloc->new(alloc);
    btree tree = (btree){alloc, tree_node};
    btree_data *tree_data = alloc->load(tree, tree_node);
    tree_data->height = 0;
    tree_data->alloc = alloc;
    // Calculate how many keys will fit in each type of node
    // TODO: check correctness, esp. in regards to padding
    tree_data->max_interior_keys = (alloc->node_size-32)
                            / (sizeof(struct{bt_pair a; void* b;})) - 1;
    tree_data->max_leaf_keys = (alloc->node_size-32) / sizeof(bt_pair) - 1;
    uint16_t max_root_keys = (alloc->node_size-32-userdata_size)
                           / (sizeof(struct{bt_pair a; void* b;})) - 1;
    //TODO root node in same page (→ also tree->height==-1 instead of !tree->root)
    // maybe also store allocator-specific data
    alloc->unload(tree, tree_node);
    return tree;
}

void *btree_load_userdata(btree tree){
    btree_data *tree_data = tree.alloc->load(tree, tree.root);
    return ((char*)tree_data)+sizeof(btree_data);
}

void btree_unload_userdata(btree tree){
    tree.alloc->unload(tree, tree.root);
}

// Returns the index of key *2+1 (even number if between indices)
// (therefore can't use bsearch())
static int search_keys(bt_node* node, bt_key key){
    int min = 0;
    int max = node->num_keys; // max not inclusive
    // binary search
    while(max-min>7){
       int median = (min+max)/2;
       if(key < node->pairs[median].key)
           max = median;
       else
           min = median;
    }
    // linear search (could maybe be removed)
    for(;min<max; min++){
        if(key < node->pairs[min].key)
            return 2*min;
        if(key == node->pairs[min].key)
            return 2*min+1;
    }
    return 2*min;
}

typedef struct {
    bool already_present;
    bt_pair pair;
    bt_node* new_node;
} insert_result;

static insert_result insert(bt_node *node, bt_pair pair, int height){
    int index = search_keys((void*)node, pair.key);
    if(index%2){ // key already present
        node->pairs[index/2].value = pair.value;
        return (insert_result){true};
    }
    bt_node *new_node;
    int child = index/2;
    if(height){
        insert_result split = 
            insert(node->children[child], pair, height-1);
        if(!split.new_node)
            return split;
        pair = split.pair;
        new_node = split.new_node;
    }
    if(node->num_keys < MAX_KEYS(height)){
        // enough room, insert new child
        for(int i = node->num_keys; i --> child;)
            node->pairs[i+1] = node->pairs[i];
        if(height) // height==0 means leave → no children
            for(int i = node->num_keys; i > child; i--)
                node->children[i+1] = node->children[i];
        node->num_keys++;
        node->pairs[child] = pair;
        if(height)
            node->children[child+1] = new_node;
        return (insert_result){true};
    } else {
        // node full
        // TODO: try to push into siblings
        
        // split node
        // don't allocate space for children if leaf
        bt_node *right = calloc(
                height?sizeof(bt_node):sizeof(bt_node_leaf), 1);
        node->num_keys = MIN_KEYS(height) + MAX_KEYS(height)%2; 
        right->num_keys = MIN_KEYS(height);
        // if the key is less than the median, insert it into the old node
        // if greater insert into the new one
        // copy half of the keys into the right node
        // deleted from the first one as num_keys is lowered
        bt_pair median;
        if(child == node->num_keys){
            //key in middle
            median = pair;
            for(int i = MAX_KEYS(height); i --> node->num_keys;)
                right->pairs[i-node->num_keys] = node->pairs[i];
            if(height)
                right->children[0] = new_node;
            if(height)
                for(int i = MAX_KEYS(height)+1; i --> node->num_keys+1;)
                    right->children[i-node->num_keys] = node->children[i];
        } else if(child <= MIN_KEYS(height)){
            //key in left node
            for(int i = MAX_KEYS(height); i --> node->num_keys;)
                right->pairs[i-node->num_keys] = node->pairs[i];
            if(height)
                for(int i = MAX_KEYS(height)+1; i --> node->num_keys;)
                    right->children[i-node->num_keys] = node->children[i];
            median = node->pairs[node->num_keys-1];
            for(int i = node->num_keys-1; i --> child;)
                node->pairs[i+1] = node->pairs[i];
            if(height)
                for(int i = node->num_keys; i --> child+1;)
                    node->children[i+1] = node->children[i];
            node->pairs[child] = pair;
            if(height)
                node->children[child+1] = new_node;
        } else {
            //key in right node
            median = node->pairs[node->num_keys];
            for(int i = child; i --> node->num_keys+1;)
                right->pairs[i-node->num_keys-1] = node->pairs[i];
            if(height)
                for(int i = child+1; i --> node->num_keys+1;)
                    right->children[i-node->num_keys-1] = node->children[i];
            right->pairs[child-node->num_keys-1] = pair;
            if(height)
                right->children[child-node->num_keys] = new_node;
            for(int i = MAX_KEYS(height); i --> child;)
                right->pairs[i-node->num_keys] = node->pairs[i];
            if(height)
                for(int i = MAX_KEYS(height)+1; i --> child+1;)
                    right->children[i-node->num_keys] = node->children[i];
        }
        return (insert_result){false, median, right};
    }
}

bool btree_insert(btree* tree, bt_key key, bt_value value){
    if(!tree->root){
        bt_node_leaf *node = calloc(sizeof(bt_node_leaf), 1);
        node->num_keys = 1;
        node->pairs[0] = (bt_pair){.key=key, .value=value};
        tree->root = (void*)node;
        return false;
    } else {
        insert_result split = insert(tree->root,
                (bt_pair){.key=key, .value=value}, tree->height);
        if(split.new_node){
            bt_node *new_root = calloc(sizeof(bt_node), 1);
            new_root->num_keys = 1;
            new_root->pairs[0] = split.pair;
            new_root->children[0] = tree->root;
            new_root->children[1] = split.new_node;
            tree->root = new_root;
            tree->height++;
        }
        return split.already_present;
    }
}



typedef struct {
    bt_value value;
    bool found;
} search_result;

static search_result search(bt_node* node, bt_key key, uint8_t height){
    int index = search_keys(node, key);
    if(index%2==1)
        // key in node->pairs
        return (search_result){node->pairs[index/2].value, true};
    if(height==0)
        // leaf node & key's not a child
        return (search_result){.found=false};
    else
        // recurse
        return search(node->children[index/2], key, height-1);
}

bool btree_contains(btree *tree, bt_key key){
    if(tree->root)
        return search(tree->root, key, tree->height).found;
    else
        return false;
}

bt_value btree_get(btree *tree, bt_key key, bool *success){
    if(tree->root){
        search_result result = search(tree->root, key, tree->height);
        *success = result.found;
        return result.value;
    } else {
        *success = false;
        return (bt_value){0};
    }
}

bt_value btree_get_or_default(btree *tree, bt_key key, bt_value alt){
    bool success;
    bt_value value = btree_get(tree, key, &success);
    return success ? value : alt;
}



static void traverse(bt_node *node,
        bt_value(*callback)(bt_key, bt_value, void*),
        void* id, bool reverse, int height){
    if(!reverse)
        for(int i=0; i <= node->num_keys; i++){
            if(height)
                traverse(node->children[i], callback, id, reverse, height-1);
            if(i<node->num_keys)
                node->pairs[i].value = 
                    callback(node->pairs[i].key, node->pairs[i].value, id);
        }
    else
        for(int i=node->num_keys+1; i --> 0;){
            if(height)
                traverse(node->children[i], callback, id, reverse, height-1);
            if(i<node->num_keys)
                node->pairs[i].value = 
                    callback(node->pairs[i].key, node->pairs[i].value, id);
        }
}

void btree_traverse(btree* tree, 
        bt_value(*callback)(bt_key, bt_value, void*),
        void* id, bool reverse){
    if(tree->root)
        traverse(tree->root, callback, id, reverse, tree->height);
}

static bt_pair find_smallest(bt_node *node, int height){
    if(!height)
        return node->pairs[0];
    else
        return find_smallest(node->children[0], height-1);
}

static bt_pair find_biggest(bt_node *node, int height){
    if(!height)
        return node->pairs[node->num_keys-1];
    else
        return find_biggest(node->children[node->num_keys], height-1);
}

static void free_node(bt_node *node, int height){
    if(height>0)
        for(int i=node->num_keys+1; i --> 0;)
            free_node(node->children[i], height-1);
    free(node);
}

void btree_delete(btree *tree){
    if(tree->root)
        free_node(tree->root, tree->height);
    free(tree);
}

static bool remove_key(bt_node *node, bt_key key, int height){
    int index = search_keys(node, key);
    if(!height){
        if(!(index%2))
            return false;
        for(int i = index/2; i < node->num_keys-1; i++)
            node->pairs[i] = node->pairs[i+1];
        // parent will check if below min number of keys
        node->num_keys--;
        return true;
    } else {
        int child = index/2;
        bool found;
        if(!(index%2)){
            found = remove_key(node->children[child], key, height-1);
        } else if(child<node->num_keys && 
                  node->children[child+1]->num_keys > 
                  node->children[child]->num_keys){
            // the smallest key in the right subtree works as seperator
            child++;
            bt_pair seperator = find_smallest(
                        node->children[child], height-1);
            node->pairs[index/2] = seperator;
            remove_key(node->children[child], seperator.key, height-1);
            found = true;
        } else {
            // the biggest key in the left subtree works as seperator
            bt_pair seperator = find_biggest(
                    node->children[child], height-1);
            node->pairs[index/2] = seperator;
            remove_key(node->children[child], seperator.key, height-1);
            found = true;
        }
        // rebalance if child below min number of keys
        bt_node *cn = node->children[child];
        if(cn->num_keys<MIN_KEYS(height-1)){
            // check immediate siblings for available key
            // take from left if possible
            if(child>0 && node->children[child-1]->num_keys>MIN_KEYS(height-1)){
                bt_node *prev = node->children[child-1];
                for(int i = cn->num_keys; i --> 0;)
                    cn->pairs[i+1] = cn->pairs[i];
                if(height-1)
                    for(int i = cn->num_keys+1; i --> 0;)
                        cn->children[i+1] = cn->children[i];
                cn->pairs[0] = node->pairs[child-1];
                node->pairs[child-1] = prev->pairs[prev->num_keys-1];
                if(height-1)
                    cn->children[0] = prev->children[prev->num_keys];
                prev->num_keys--;
                cn->num_keys++;
            }
            // else take from right if possible
            else if(child<node->num_keys && 
                    node->children[child+1]->num_keys>MIN_KEYS(height-1)){
                bt_node *next = node->children[child+1];
                cn->pairs[cn->num_keys] = node->pairs[child];
                node->pairs[child] = next->pairs[0];
                for(int i = 0; i < next->num_keys-1; i++)
                    next->pairs[i] = next->pairs[i+1];
                if(height-1){
                    cn->children[cn->num_keys+1] = next->children[0];
                    for(int i = 0; i < next->num_keys; i++)
                        next->children[i] = next->children[i+1];
                }
                next->num_keys--;
                cn->num_keys++;
            } else {
                // if none available in siblings, merge
                // make sure it works both when child is leftmost and rightmost
                bt_node *left;
                if(child == 0){
                    left = node->children[0];
                    cn = node->children[1];
                } else {
                    left = node->children[child-1];
                    child--;
                }
                left->pairs[left->num_keys] = node->pairs[child];
                for(int i = child; i < node->num_keys-1; i++)
                    node->pairs[i] = node->pairs[i+1];
                for(int i = child+1; i < node->num_keys; i++)
                    node->children[i] = node->children[i+1];
                for(int i = cn->num_keys; i --> 0;)
                    left->pairs[i+left->num_keys+1] = cn->pairs[i];
                if(height-1)
                    for(int i = cn->num_keys+1; i --> 0;) 
                        left->children[i+left->num_keys+1] = cn->children[i];
                left->num_keys += 1 + cn->num_keys;
                node->num_keys--;
                free(cn);
            }
        }
        return found;
    }
}

bool btree_remove(btree* tree, bt_key key){
    if(tree->root){
        bool found = remove_key(tree->root, key, tree->height);
        // root may have fewer than MIN_KEYS keys
        if(tree->root->num_keys==0){
            if(tree->height==0){
                tree->root = NULL;
                free(tree->root);
            } else {
                bt_node *new_root = tree->root->children[0];
                free(tree->root);
                tree->root = new_root;
                tree->height--;
            }
        }
        return found;
    } else
        return false;
}



// Recursove function, to be called only by btree_debug_print() (and itself).
// Height is the distance to the leafs, max_height is the height of the root,
// startc is a graph line connection character (unicode), lines_above and _below
// are bitmask for whether to draw vertical lines at the given height
static void debug_print(FILE *stream, bt_node* node, bool print_value, int height, int max_height, char *startc, uint32_t lines_above, uint32_t lines_below){
    for(int i = 0; i < node->num_keys+1; i++){
        // Print child
        if(height){
            uint32_t lines_row = i<(node->num_keys+1)/2?lines_above:lines_below;
            debug_print(stream, node->children[i], print_value, height-1, max_height,
                    i==0 ?              "╭"
                  : i==node->num_keys ? "╰"
                  :                     "├",
                  lines_row|(i==0?0:1<<(max_height-height)),
                  lines_row|(i==node->num_keys?0:1<<(max_height-height)));
        }
        // Print key
        if(i<node->num_keys){
            // Print space and horizontal lines
            for(int s = 0; s<(max_height-height)*6; s++)
                fputs(s%6==5 && (i<node->num_keys/2?lines_above:lines_below)&1<<(s/6) ?"│":" ", stream);
            // Print horizontal lines & connectors
            if(!height){ // leaf nodes
                if(i==0 && node->num_keys==1)
                    fprintf(stream, "\033[D%s──────", startc);
                else if(i==0 && node->num_keys==2)
                    fprintf(stream, "\033[D%s─────┬", startc);
                else if(i==0)
                    fprintf(stream, "     ╭");
                else if(i==(node->num_keys-1)/2)
                    fprintf(stream, "\033[D%s─────┤", startc);
                else if(i==node->num_keys-1)
                    fprintf(stream, "     ╰");
                else
                    fprintf(stream, "     │");
            } else { // interior nodes
                if(i==(node->num_keys-1)/2)
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
                        node->pairs[i].key, node->pairs[i].value);
            else
                fprintf(stream, "%x\n", node->pairs[i].key);
        }
    }
}

void btree_debug_print(FILE *stream, btree* tree, bool print_value){
    if(tree->root)
        debug_print(stream, tree->root, print_value, tree->height, tree->height, "", 0, 0);
    else
        puts("(empty)");
}
