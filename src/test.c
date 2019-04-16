#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "btree.h"
#include "btree-debug.h"

struct mark_occurences {
    bt_key *keys;
    int *occurences;
    int count;
};

bt_value callback(bt_key key, bt_value value, void *data){
    struct mark_occurences *d = data;
    for(int i = d->count; i --> 0;)
        if(key == d->keys[i])
            d->occurences[i]++;
    return value;
}

void test_random(int len, int attempts){
    for(int a = 0; a < attempts; a++){
        btree *tree = btree_new();

//        printf("TEST KEYS%d:\n", a);
        bt_key *keys = calloc(sizeof(bt_key), len);
        int *occurences = calloc(sizeof(int), len);
        for(int i=0; i < len; i++){
            bt_key key = rand()%0xff;
//            printf("%x\n", key);
            keys[i] = key;
        }
        for(int i=0; i < len; i++){
            btree_insert(tree, keys[i], NULL);
//            printf("Inserted %x, ", keys[i]);
//            btree_debug_print(tree);
        }
        
        struct mark_occurences *result = 
            calloc(sizeof(struct mark_occurences), 1);
        result->keys = keys;
        result->occurences = occurences;
        result->count = len;
        btree_traverse(tree, callback, result, false);
    
        for(int i = len; i --> 0;)
            occurences[i]--;

        for(int i=len; i --> 0;)
            if(occurences[i]){
                puts("KEYS");
                for(int j=len; j --> 0; printf("  %x\n", keys[j]));
                printf("Incorrect: %x deviating by %d\n", keys[i], occurences[i]);
                exit(1);
            }

        btree_free(tree);
    }
}

// for testing specific cases
void test_failcase(){
    btree *tree = btree_new();
    bt_key keys[] = {
        0,
    };
    //for(unsigned long i=0; i < sizeof(keys)/sizeof(bt_key); i++){
    for(unsigned long i = sizeof(keys)/sizeof(bt_key); i --> 0;){
        btree_insert(tree, keys[i], NULL);
        printf("Inserting %x, ", keys[i]);
        btree_debug_print(tree);
    }
}

int main(void){
    
    time_t t;
    srand((unsigned) time(&t));
//    srand(1);

    test_random(56, 20);
//    test_failcase();

    return 0;
}

