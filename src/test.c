#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include "btree.h"

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

void test_random(int len, float del_chance, int attempts){
    for(int a = 0; a < attempts; a++){
        btree *tree = btree_new();

        printf("TEST KEYS %d:\n", a);
        bt_key *keys = calloc(sizeof(bt_key), len);
        int *occurences = calloc(sizeof(int), len);
        int *correct = calloc(sizeof(int), len);
        bool *ops = calloc(sizeof(int), len);
        for(int i=0; i < len; i++){
            bt_key key = rand()%0x1ffffe + 1;
            bool delete = ((float)rand())/(float)RAND_MAX < del_chance;
            ops[i] = delete;
            printf("%d   %s: %x\n\n", i, delete?"del":"add", key);
            if(a==485 && i==12){
                puts("Problem");
            }
            if(delete)
                btree_delete(tree, key);
            else
                btree_insert(tree, key, NULL);
            btree_debug_print(stdout, tree, false);
            keys[i] = key;
            for(int j = 0; j<=i; j++)
                if(keys[j]==key)
                    correct[j] = 1-delete;
        }
        
        struct mark_occurences *result = 
            calloc(sizeof(struct mark_occurences), 1);
        result->keys = keys;
        result->occurences = occurences;
        result->count = len;
        btree_traverse(tree, callback, result, false);
    
        for(int i=len; i --> 0;)
            if(occurences[i] != correct[i]){
                puts("KEYS");
                for(int j=len; j --> 0;)
                    printf("%s %x\n", ops[j]?"del":"add",keys[j]);
                printf("Incorrect: %x deviating by %d\n", keys[i], 
                        occurences[i]-correct[i]);
//            btree_debug_print(tree);
                exit(1);
            }

        btree_free(tree);
        free(keys);
        free(occurences);
        free(correct);
    }
}

int main(void){
//    time_t t;
//    srand((unsigned) time(&t));
    srand(1);
    
    test_random(100, 0, 1);
//    test_random(15, 0.2, 486);

    return 0;
}
