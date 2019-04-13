#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "btree.h"
#include "btree-debug.h"

void test_random(int len, int attempts){
    while(attempts --> 0){
        btree *tree = btree_new();

        bt_key *keys = calloc(sizeof(bt_key), len);
        for(int i=0; i < len; i++){
            bt_key key = rand()%100;
            btree_insert(tree, key);
            keys[i] = key;
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
    }
}

void test_failcase(){
    btree *tree = btree_new();
    bt_key keys[] = {34, 33, 35, 9, 1, 6, 3, 5, 4, 2};
    for(unsigned long i=0; i < sizeof(keys)/sizeof(bt_key); i++){
        btree_insert(tree, keys[i]);
        btree_debug_print(tree);
    }
}

int main(void){
    
    time_t t;
    srand((unsigned) time(&t));

    test_random(10, 20);
//    test_failcase();

    return 0;
}

