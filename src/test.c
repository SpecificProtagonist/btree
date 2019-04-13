#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "btree.h"
#include "btree-debug.h"

void test_random(int len, int attempts){
    while(attempts --> 0){
        btree *tree = btree_new();

        puts("TEST:");
        bt_key *keys = calloc(sizeof(bt_key), len);
        for(int i=0; i < len; i++){
            bt_key key = rand()%100;
            btree_insert(tree, key);
            keys[i] = key;
            printf("  %x\n", key);
        }
    
        for(int i=len; i --> 0;)
            if(!btree_contains(tree, keys[i])){
                puts("KEYS");
                for(int j=len; j --> 0; printf("  %d\n", keys[j]));
                printf("Missing: %d\n", keys[i]);
                puts("TREE");
                btree_debug_print(tree);
                exit(1);
            }

        btree_free(tree);
    }
}

void test_failcase(){
    btree *tree = btree_new();
    bt_key keys[] = {0x5, 0x2, 0x7, 0x1, 0xB, 0x9, 0x3, 0xA, 0x6, 0x4, 0x8};
    for(unsigned long i=0; i < sizeof(keys)/sizeof(bt_key); i++){
        btree_insert(tree, keys[i]);
        btree_debug_print(tree);
    }
}

int main(void){
    
    time_t t;
    srand((unsigned) time(&t));

//    test_random(20, 10);
    test_failcase();

    return 0;
}

