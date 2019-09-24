A b-tree implementation in C for the seminar "Exploring Datastructures in C" at TU Bergakademie Freiberg.

The trees may contain keys & values of arbitrary size and can exist either purely in RAM, or in file, in which case the neccessary nodes are mmap'ed.

Usage of all functions is documented in btree.h. For example, to create a new tree in RAM:
```
// An allocator simply manages memory for a btree. 
// The first argument here is the node size in bytes, the second is an optional error callback.
bt_alloc_ptr alloc = btree_new_ram_alloc(512, NULL);

// Create a new tree
// Say we want to store 32-character strings and retrieve them by integer key.
// We could also specify a function used for comparing the keys incase we want to
// be able traverse the stored key-value pairs in a specific order.
// The last parameter allows us to store some data alongside the tree,
// which can be useful if the tree is stored in a file.
btree tree = btree_create(alloc, sizeof(int), 32, memcmp, 0);

// Insert a key/value pair
int id = 244321;
char name[32] = "AzureDiamond";
btree_insert(tree, &id, &name)

// Or retrieve it
bool found = btree_get(tree, &id, &name);
```

Or backed by a file (the zeros mean no extra data stored alongside):
```
int fd = open("some_file", ORDWR);
bt_alloc_ptr alloc = btree_new_file_alloc(fd, NULL, 0, NULL);

// The same file can harbor multiple trees
btree tree_1 = btree_create(alloc, sizeof(int), 32, memcmp, 0);
btree tree_2 = btree_create(alloc, sizeof(int), 32, memcmp, 0);
```

Maybe you want to print out all pairs and count them while you're at it?
Create a function that will be called for each pair:
```
bool callback(const void *key, void *value, void *count){
    printf("ID: %d   Username: %s\n", *(int*)key, value);
    *(int*)count += 1;
    // returning false means we want to continue the traversal
    return false;
}
```
And traverse the tree:
```
int count = 0;
btree_traverse(tree, callback, &count, false);
```



Currently, trees are not multithreading safe and the project has only been tested on Linux with gcc.

To build, simply use `make`.
To use, include `btree.h` and link against `btree` (build/release/libbtree.a).
