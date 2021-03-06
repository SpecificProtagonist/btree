#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "btree.h"


struct mark_occurences {
    uint32_t *keys;
    int *occurences;
    int count;
};

bool occurence_callback(const void *key, void *value, void *params){
    struct mark_occurences *data = params;
    for(int i = data->count; i --> 0;)
        if(*(uint32_t*)key == data->keys[i])
            data->occurences[i]++;
    return false;
}

typedef struct {
    btree tree;
    uint32_t last_key;
} order_helper;

bool order_callback(const void *key, void *value, void *params){
    order_helper *par = (order_helper*) params;
    //if(memcmp(&par->last_key, key, sizeof(uint32_t))>=0){
    if(par->last_key >= *(uint32_t*)key){
        printf("TEST FAILED:\nKey %x appeared before key %x\n",
                par->last_key, *(uint32_t*)key);
        btree_debug_print(stderr, par->tree, NULL, NULL);
        exit(1);
    }
    par->last_key = *(uint32_t*)key;
    return false;
}

bool value_callback(const void *key, void *value, void *params){
    if(*(uint32_t*)key!=*(uint32_t*)value){
        printf("TEST FAILED:\nKey %x has value %x\n", 
                *(uint32_t*)key, *(uint32_t*)value);
        btree_debug_print(stderr, *(btree*)params, NULL, NULL);
        exit(1);
    }
    return false;
}

int compare_uint32(const void *key1, const void *key2, size_t size){
    if(*(uint32_t*)key1 < *(uint32_t*)key2) return -1;
    if(*(uint32_t*)key1 > *(uint32_t*)key2) return 1;
    else return 0;
}

void test_random(bt_alloc_ptr alloc, int attempts, int len, float del_chance){
    for(int att = 1; att <= attempts; att++){
        bool debug = false;
        
        if(debug)
            printf("# Del chance %f, iteration %d\n", del_chance, att);

        // TODO: test userdata storage
        btree tree = btree_create(alloc, sizeof(uint32_t), sizeof(uint32_t),
                        compare_uint32, 0);

        // Create random tree
        uint32_t *keys = calloc(sizeof(uint32_t), len);
        int *occurences = calloc(sizeof(int), len);
        int *correct = calloc(sizeof(int), len);
        bool *ops = calloc(sizeof(int), len);
        for(int i=0; i < len; i++){
            uint32_t key = rand()%(3*len) + 1;
            bool delete = ((float)rand())/(float)RAND_MAX < del_chance;
            ops[i] = delete;
            if(debug)
                printf("%d | %s %x (%d)\n", i, delete?"Deleting":"Inserting", key, key);
            if(delete)
                btree_remove(tree, &key, NULL);
            else
                btree_insert(tree, &key, &key);
            if(debug)
                btree_debug_print(stdout, tree, NULL, NULL);
            keys[i] = key;
            for(int j = 0; j<=i; j++) if(keys[j]==key)
                    correct[j] = 1-delete;
        }

        btree_debug_print(stdout, tree, NULL, NULL);
        exit(0);

        // Check if the tree containes each key the correct amount of times
        struct mark_occurences *result = 
            calloc(sizeof(struct mark_occurences), 1);
        result->keys = keys;
        result->occurences = occurences;
        result->count = len;
        btree_traverse(tree, occurence_callback, result, false);

        for(int i=len; i --> 0;)
            if(occurences[i] != correct[i]){
                printf("TEST FAILED:\n");
                for(int j=len; j --> 0;)
                    printf("%s %x\n", ops[j]?"del":"add",keys[j]);
                printf("Incorrect: %x occured %d instead of %d times\n", keys[i],
                        occurences[i], correct[i]);
                btree_debug_print(stdout, tree, NULL, NULL);
                exit(1);
            }
        
        // Check that key order isn't violated
        order_helper *order = malloc(sizeof(order_helper));
        order->tree = tree;
        order->last_key = 0;
        btree_traverse(tree, order_callback, order, false);

        // Check that each value is preserved
        btree_traverse(tree, value_callback, &tree, false);
        

        btree_delete(tree);
        free(keys);
        free(occurences);
        free(correct);
    }
}

void create_tree(){
    int file = open("stored", O_RDWR);
    if(file==-1){
        perror("Couldn't open file");
        exit(1);
    }

    bt_alloc_ptr alloc = btree_new_file_alloc(file, NULL, 0, NULL);
//    bt_alloc_ptr alloc = btree_new_ram_alloc(100);
    btree tree = btree_create(alloc, 4, 4, memcmp, 0);
    int num = 3500;
    btree_debug_print(stdout, tree, NULL, NULL);
    for(int i = 1; i < num; i++){
//        printf("Insertion iteration %x\n", i);
        btree_insert(tree, &i, &i);
//        btree_debug_print(stdout, tree, true);
    }
    close(file);
}

void remove_tree(){
    int file = open("stored", O_RDWR);
    if(file==-1){
        perror("Couldn't open file");
        exit(1);
    }

    bt_alloc_ptr alloc = btree_load_file_alloc(file, NULL, NULL);
    btree tree = {alloc, 1, memcmp};

    int num = 3500;
    for(int i = 1; i < num; i++){
//        printf("Deletion iteration %x\n", i);
        uint32_t value;
        if(!btree_remove(tree, &i, &value)){
            printf("Tree did not contain %x\n", i);
            exit(1);
        }
        if(value!=i){
            printf("Value for key %x was %x\n", i, value);
            exit(1);
        }
//        btree_debug_print(stdout, tree, true);
    }
    close(file);
}

int main(void){
    //time_t t;
    //srand((unsigned) time(&t));
    srand(1);
  
//    test_serialization();

//    create_tree();
//    remove_tree();

//    bt_alloc_ptr alloc = btree_new_ram_alloc(496);
    bt_alloc_ptr alloc = btree_new_ram_alloc(100, NULL);
    for(float del_chance = 0.1; del_chance < 0.6; del_chance += 0.15)
        test_random(alloc, 3000, 250, del_chance);
//            test_random(alloc, 400, 0.25);

    return 0;
}
