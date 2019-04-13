#include <stdlib.h>
#include <stdio.h>
#include "btree.h"

// store order as property of the btree or as compile time option?
// 32 or so might be sensible if kept in RAM, 3 good for testing
// Number of children is number of keys + 1
#ifndef MAX_KEYS
#define MAX_KEYS 3
#endif
#define MIN_KEYS (MAX_KEYS/2)

typedef struct bt_node bt_node;
struct bt_node {
    uint8_t num_keys;
    bt_key keys[MAX_KEYS];
    bt_node* children[MAX_KEYS+1];
};

typedef struct {
    uint8_t num_keys;
    bt_key keys[MAX_KEYS];
} bt_node_leaf;

struct btree {
   bt_node* root;
   int8_t height;
};

btree* btree_new(){
    return calloc(sizeof(btree), 1);
}

/*
 *  Returns the index of key *2+1 (even number if between indices)
 *  (therefore can't use bsearch())
 */
int search_keys(bt_node* node, bt_key key){
    int min = 0;
    int max = node->num_keys;
    //while(max-min>7){
       // do binary search 
    //}
    for(;min<max; min++){
        if(key < node->keys[min])
            return 2*min;
        if(key == node->keys[min])
            return 2*min+1;
    }
    return 2*min;
}

typedef struct {
    bt_key split_key;
    bt_node* new_node;
} insert_result;

insert_result insert_into_leaf(bt_node_leaf* leaf, bt_key key){
    int index = search_keys((void*)leaf, key);
    if(index%2==1){
        // key already present
        return (insert_result){0};
    } else if(leaf->num_keys < MAX_KEYS){ // space free
        for(int i = leaf->num_keys; i --> index/2;)
            leaf->keys[i+1] = leaf->keys[i];
        leaf->keys[index/2] = key;
        leaf->num_keys++;
        return (insert_result){0};
    } else { // node full → split node
        bt_node_leaf *right = calloc(sizeof(bt_node_leaf), 1);
        leaf->num_keys = MIN_KEYS + MAX_KEYS%2;
        right->num_keys = MIN_KEYS;
        // if the key is less than the median, insert it into the old node
        // if greater insert into the new one
        // copy half of the keys into the right node
        // deleted from the first one as num_keys is lowered
        bt_key median;
        if(index/2 == leaf->num_keys) {
            median = key;
            for(int i = MAX_KEYS; i --> leaf->num_keys;)
                right->keys[i-leaf->num_keys] = leaf->keys[i];
        } else if(index/2 <= MIN_KEYS){
            median = leaf->keys[MIN_KEYS];
            for(int i = leaf->num_keys-1; i --> index/2;)
                leaf->keys[i+1] = leaf->keys[i];
            leaf->keys[index/2] = key;
            for(int i = MAX_KEYS; i --> leaf->num_keys;)
                right->keys[i-leaf->num_keys] = leaf->keys[i];
        } else {
            median = leaf->keys[MIN_KEYS+1];
            // is this correct for both even and odd MAX_KEYS?
            for(int i = index/2-1; i --> leaf->num_keys;)
                right->keys[i-leaf->num_keys] = leaf->keys[i];
            right->keys[index/2-leaf->num_keys-1] = key;
            for(int i = MAX_KEYS; i --> index/2;)
                right->keys[i-leaf->num_keys] = leaf->keys[i];
        }
        return (insert_result){median, (void*)right};
    }
}

// perhaps combine with insert_into_leaf() and insert()?
insert_result child_split(bt_node* node, int child, insert_result split){
    if(node->num_keys < MAX_KEYS){
        // enough room, insert new child
        for(int i = node->num_keys; i --> child;)
            node->keys[i+1] = node->keys[i];
        for(int i = node->num_keys; i > child; i--)
            node->children[i+1] = node->children[i];
        node->num_keys++;
        node->keys[child] = split.split_key;
        node->children[child+1] = split.new_node;
        return (insert_result){0};
    } else { // split node
        bt_node *right = calloc(sizeof(bt_node), 1);
        node->num_keys = MIN_KEYS + MAX_KEYS%2;
        right->num_keys = MIN_KEYS;
        bt_key median;
        if(child == node->num_keys){ //key in middle
            median = split.split_key;
            for(int i = MAX_KEYS; i --> node->num_keys;)
                right->keys[i-node->num_keys] = node->keys[i];
            right->children[0] = split.new_node;
            for(int i = MAX_KEYS+1; i --> node->num_keys+1;)
                right->children[i-node->num_keys] = node->children[i];
        } else if(child <= MIN_KEYS){ //key in left node
            for(int i = MAX_KEYS; i --> node->num_keys;)
                right->keys[i-node->num_keys] = node->keys[i];
            for(int i = MAX_KEYS+1; i --> node->num_keys;)
                right->children[i-node->num_keys] = node->children[i];
            median = node->keys[MIN_KEYS];
            for(int i = node->num_keys-1; i --> child;)
                node->keys[i+1] = node->keys[i];
            for(int i = node->num_keys; i --> child+1;)
                node->children[i+1] = node->children[i];
            node->keys[child] = split.split_key;
            node->children[child+1] = split.new_node;
        } else { //key in right node
            median = node->keys[node->num_keys];
            for(int i = child; i --> node->num_keys+1;)
                right->keys[i-MIN_KEYS-1] = node->keys[i];
            for(int i = child+1; i --> node->num_keys+1;)
                right->keys[i-MIN_KEYS-1] = node->keys[i];
            right->keys[child-node->num_keys-1] = split.split_key;
            for(int i = MAX_KEYS; i --> child+1;)
                right->keys[i-MIN_KEYS] = node->keys[i];
        }
        return (insert_result){median, right};
    }
}

insert_result insert(bt_node* node, bt_key key, uint8_t height){
    if(height==0){
        return insert_into_leaf((void*)node, key);
    } else {
        int index = search_keys(node, key);
        if(index%2==1){
            return (insert_result){0}; // key already present
        } else {
            insert_result result 
                = insert(node->children[index/2], key, height-1);
            if(result.new_node)
                return child_split(node, index/2, result);
            else
                return (insert_result){0};
        }
    }
}

void btree_insert(btree* tree, bt_key key){
    if(!tree->root){
        bt_node_leaf *node = calloc(sizeof(bt_node_leaf), 1);
        node->num_keys = 1;
        node->keys[0] = key;
        tree->root = (void*)node;
    } else {
        insert_result split = insert(tree->root, key, tree->height);
        if(split.new_node){
            bt_node *new_root = calloc(sizeof(bt_node), 1);
            new_root->num_keys = 1;
            new_root->keys[0] = split.split_key;
            new_root->children[0] = tree->root;
            new_root->children[1] = split.new_node;
            tree->root = new_root;
            tree->height++;
        }
    }
}

bool contains(bt_node* node, bt_key key, uint8_t height){
    int index = search_keys(node, key);
    if(index%2==1)
        return true; //key in node->keys
    if(height==0)
        return false; //leaf node & not key's not a child
    else
        return contains(node->children[index/2], key, height-1); //recurse
}

bool btree_contains(btree* tree, bt_key key){
    if(tree->root)
        return contains(tree->root, key, tree->height);
    else
        return false;
}

void btree_traverse(btree*, void (*callback)(bt_key), bool reverse);

void btree_delete(btree*, bt_key key);

void free_node(bt_node *node, int height){
    if(height>0)
        for(int i=node->num_keys+1; i --> 0;)
            free_node(node->children[i], height-1);
    free(node);
}

void btree_free(btree *tree){
    if(tree->root)
        free_node(tree->root, tree->height);
    free(tree);
}

void debug_print(bt_node* node, int height, int max_height){
    for(int i = 0; i < node->num_keys+1; i++){
        if(height>0)
            debug_print(node->children[i], height-1, max_height);
        if(i<node->num_keys){
            for(int s = (max_height-height)*4; s --> 0; printf(" "));
            printf("%x\n", node->keys[i]);
        }
    }
}

void btree_debug_print(btree* tree){
    printf("Tree height: %d\n", tree->height);
    if(tree->root)
        debug_print(tree->root, tree->height, tree->height);
    else
        puts("EMPTY");
}
