#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <strings.h>
#include "map.h"

#define countof(a) (sizeof((a))/sizeof(*(a)))
#define null NULL

static void dump_strstr_map(void* that, const map_entry_t* e) {
    printf("\"%s\"=\"%s\"\n", e->key, e->val);
}

static void map_naive_smoke_test() {
    enum { INITIAL_CAPACITY = 4 };
    const char* keys[] = {
        "a",
        "aa",
        "ab",
        "aaa",
        "aab",
        "Much longer key",
        "Abraca",
        "Hello",
        "Goodbye"
    };
    const char* vals[] = {
        "z",
        "zz",
        "zx",
        "zzz",
        "zzx",
        "With a bit longer value",
        "Dabra",
        "World",
        "Universe"
    };
    map_t m = map_create(INITIAL_CAPACITY);
    for (int i = 0; i < countof(keys); i++) {
//      int h = map_hashcode(keys[i], strlen(keys[i]) + 1);
//      printf("map_hashcode(\"%s\") = %d\n", keys[i], h % map_capacity(m));
        map_put(m, keys[i], strlen(keys[i]) + 1, vals[i], strlen(vals[i]) + 1);
        map_entry_t e = map_get(m, keys[i], strlen(keys[i]) + 1);
        printf("map[\"%s\"] = \"%s\" size=%d capacity=%d\n", keys[i], e.val, map_size(m), map_capacity(m));
    }
    printf("----\n");
    map_iterate(m, m, dump_strstr_map);
    printf("----\n");
    for (int i = 0; i < countof(keys); i++) {
        map_entry_t e = map_get(m, keys[i], strlen(keys[i]) + 1);
        printf("map[\"%s\"] = \"%s\" size=%d capacity=%d\n", keys[i], e.val, map_size(m), map_capacity(m));
        map_remove(m, keys[i], strlen(keys[i]) + 1);
    }
    map_destroy(m);    
}

int main(int argc, char** argv) {
    map_naive_smoke_test();
    return 0;
}    