#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "btree.h"

#include <unistd.h>

struct mark_occurences {
    bt_key *keys;
    int *occurences;
    int count;
};

bt_value occurence_callback(bt_key key, bt_value value, void *params){
    struct mark_occurences *data = params;
    for(int i = data->count; i --> 0;)
        if(key == data->keys[i])
            data->occurences[i]++;
    return value;
}

typedef struct {
    btree *tree;
    bt_key last_key;
} order_helper;

bt_value order_callback(bt_key key, bt_value value, void *params){
    order_helper *par = (order_helper*) params;
    if(par->last_key>=key){
        printf("TEST FAILED:\nKey %x appeared before key %x\n",
                par->last_key, key);
        btree_debug_print(stderr, par->tree, false);
        exit(1);
    }
    par->last_key = key;
    return value;
}

bt_value value_callback(bt_key key, bt_value value, void *params){
    if(key!=(bt_key)value){
        printf("TEST FAILED:\nKey %x has value %x\n", key, value);
        btree_debug_print(stderr, (btree*)params, false);
        exit(1);
    }
    return value;
}

void test_random(int len, float del_chance){
    btree *tree = btree_new();

    // Create random tree
    bt_key *keys = calloc(sizeof(bt_key), len);
    int *occurences = calloc(sizeof(int), len);
    int *correct = calloc(sizeof(int), len);
    bool *ops = calloc(sizeof(int), len);
    for(int i=0; i < len; i++){
        bt_key key = rand()%(3*len) + 1;
        bool delete = ((float)rand())/(float)RAND_MAX < del_chance;
        ops[i] = delete;
        if(delete)
            btree_delete(tree, key);
        else
            btree_insert(tree, key, key);
        keys[i] = key;
        for(int j = 0; j<=i; j++) if(keys[j]==key)
                correct[j] = 1-delete;
    }

    // Check if the tree containes each key the correct amount of times
    struct mark_occurences *result = 
        calloc(sizeof(struct mark_occurences), 1);
    result->keys = keys;
    result->occurences = occurences;
    result->count = len;
    btree_traverse(tree, occurence_callback, result, false);

    for(int i=len; i --> 0;)
        if(occurences[i] != correct[i]){
            printf("TEST FAILED:");
            for(int j=len; j --> 0;)
                printf("%s %x\n", ops[j]?"del":"add",keys[j]);
            printf("Incorrect: %x occured %d instead of %d times\n", keys[i],
                    occurences[i], correct[i]);
            btree_debug_print(stdout, tree, false);
            exit(1);
        }
    
    // Check that key order isn't violated
    order_helper *order = malloc(sizeof(order_helper));
    order->tree = tree;
    order->last_key = 0;
    btree_traverse(tree, order_callback, order, false);

    // Check that each value is preserved
    btree_traverse(tree, value_callback, tree, false);
    

    btree_free(tree);
    free(keys);
    free(occurences);
    free(correct);
}

int main(void){
    time_t t;
    srand((unsigned) time(&t));
    //srand(1);
    
    for(float del_chance = 0.1; del_chance < 0.6; del_chance += 0.1)
        for(int a = 3000; a --> 0;){
            test_random(40, 0.25);
            test_random(400, 0.25);
        }

    return 0;
}
