#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "btree.h"
#include "btree-debug.h"

void test_random(int len, int attempts){
    for(int a = 0; a < attempts; a++){
        btree *tree = btree_new();

        printf("TEST %d:\n", a);
        bt_key *keys = calloc(sizeof(bt_key), len);
        for(int i=0; i < len; i++){
            bt_key key = rand()%256;
            printf("%x\n", key);
            keys[i] = key;
        }
        for(int i=0; i < len; i++){
            btree_insert(tree, keys[i]);
            printf("Inserted %x, ", keys[i]);
            btree_debug_print(tree);
        }
    
        for(int i=len; i --> 0;)
            if(!btree_contains(tree, keys[i])){
                puts("KEYS");
                for(int j=len; j --> 0; printf("  %x\n", keys[j]));
                printf("Missing: %x\n", keys[i]);
                puts("TREE");
                btree_debug_print(tree);
                exit(1);
            }

        btree_free(tree);
    }
}

void test_failcase(){
    btree *tree = btree_new();
    bt_key keys[] = {
        0,
    };
    //for(unsigned long i=0; i < sizeof(keys)/sizeof(bt_key); i++){
    for(unsigned long i = sizeof(keys)/sizeof(bt_key); i --> 0;){
        btree_insert(tree, keys[i]);
        printf("Inserting %x, ", keys[i]);
        btree_debug_print(tree);
    }
}

int main(void){
    
    time_t t;
    srand((unsigned) time(&t));
//    srand(1);

    test_random(176, 70);
//    test_failcase();

    return 0;
}

