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
    uint16_t num_keys;
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

insert_result insert(bt_node *node, bt_key key, int height){
    int index = search_keys((void*)node, key);
    if(index%2){ // key already present
        return (insert_result){0};
    }
    bt_node *new_node;
    int child = index/2;
    if(height){
        insert_result split = 
            insert(node->children[child], key, height-1);
        if(!split.new_node)
            return (insert_result){0};
        key = split.split_key;
        new_node = split.new_node;
    }
    if(node->num_keys < MAX_KEYS){
        // enough room, insert new child
        for(int i = node->num_keys; i --> child;)
            node->keys[i+1] = node->keys[i];
        if(height) // height==0 means leave → no children
            for(int i = node->num_keys; i > child; i--)
                node->children[i+1] = node->children[i];
        node->num_keys++;
        node->keys[child] = key;
        if(height)
            node->children[child+1] = new_node;
        return (insert_result){0};
    } else { // node full → split node
        // don't allocate space for children if leaf
        bt_node *right = calloc(
                height?sizeof(bt_node):sizeof(bt_node_leaf), 1);
        node->num_keys = MIN_KEYS + MAX_KEYS%2; 
        right->num_keys = MIN_KEYS;
        // if the key is less than the median, insert it into the old node
        // if greater insert into the new one
        // copy half of the keys into the right node
        // deleted from the first one as num_keys is lowered
        bt_key median;
        if(child == node->num_keys){
            //key in middle
            median = key;
            for(int i = MAX_KEYS; i --> node->num_keys;)
                right->keys[i-node->num_keys] = node->keys[i];
            if(height)
                right->children[0] = new_node;
            if(height)
                for(int i = MAX_KEYS+1; i --> node->num_keys+1;)
                    right->children[i-node->num_keys] = node->children[i];
        } else if(child <= MIN_KEYS){
            //key in left node
            for(int i = MAX_KEYS; i --> node->num_keys;)
                right->keys[i-node->num_keys] = node->keys[i];
            if(height)
                for(int i = MAX_KEYS+1; i --> node->num_keys;)
                    right->children[i-node->num_keys] = node->children[i];
            median = node->keys[node->num_keys-1];
            for(int i = node->num_keys-1; i --> child;)
                node->keys[i+1] = node->keys[i];
            if(height)
                for(int i = node->num_keys; i --> child+1;)
                    node->children[i+1] = node->children[i];
            node->keys[child] = key;
            if(height)
                node->children[child+1] = new_node;
        } else {
            //key in right node
            median = node->keys[node->num_keys];
            for(int i = child; i --> node->num_keys+1;)
                right->keys[i-node->num_keys-1] = node->keys[i];
            if(height)
                for(int i = child+1; i --> node->num_keys+1;)
                    right->children[i-node->num_keys-1] = node->children[i];
            right->keys[child-node->num_keys-1] = key;
            if(height)
                right->children[child-node->num_keys] = new_node;
            for(int i = MAX_KEYS; i --> child;)
                right->keys[i-node->num_keys] = node->keys[i];
            if(height)
                for(int i = MAX_KEYS+1; i --> child+1;)
                    right->children[i-node->num_keys] = node->children[i];
        }
        return (insert_result){median, right};
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

void traverse(bt_node *node, void (*callback)(bt_key, void*), void* id, bool reverse, int height){
    if(!reverse)
        for(int i=0; i <= node->num_keys; i++){
            if(height)
                traverse(node->children[i], callback, id, reverse, height-1);
            if(i<node->num_keys)
                callback(node->keys[i], id);
        }
    else
        for(int i=node->num_keys+1; i --> 0;){
            if(height)
                traverse(node->children[i], callback, id, reverse, height-1);
            if(i<node->num_keys)
                callback(node->keys[i], id);
        }
}

void btree_traverse(btree* tree, void (*callback)(bt_key, void*), void* id, bool reverse){
    if(tree->root)
        traverse(tree->root, callback, id, reverse, tree->height);
}

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
