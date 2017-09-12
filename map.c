/* Copyright (c) 1984, (leo) Kronos Group
   Copyright (c) 2017, Leo Kuznetsov (C-reimplementation)
   All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the Kronos Project.
*/

#include "map.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

/* Map  was initially implemented as a part of Modula-2 
   compiler for Kronos workstation in 1983-1984. Original code can be found at:
   https://github.com/leok7v/kronos/blob/master/excelsior/src/sys/util/mx/mxScan.m
   This is result of a weekend project re-implementing it in C.  
*/

#define countof(a) (sizeof((a))/sizeof(*(a)))
#define null NULL

enum { 
    MAX_OCCUPANCY_PERCENTAGE = 61 // 61% 
}; 

typedef struct entry_s {
    struct entry_s* next;
    struct entry_s* prev;
    int key_size;
    int val_size;
    int hash;
    unsigned char kv[1]; // [key_size + val_size]
} entry_t;

typedef struct map_s {
    int capacity;
    int size;
    map_i64_t modification_count;
    map_heap_t* heap;
    entry_t* entries[1]; // [capacity]
} **map_t;

int map_hashcode(const void* key, const int key_size) {
    int h = 5381;
    int n = key_size;
    const unsigned char* k = (const unsigned char*)key;
    while (n > 0) {
        h = ((h << 5) + h) + *k++; /* hash * 33 + c */
        n--;
    }
    h ^= key_size; // Mummur3 finalization mix (fmix32): force all bits of a hash block to avalanche
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    h  = h & 0x7FFFFFFF;   // only positive
    return h == 0 ? 1 : h; // never 0
}

static inline int key_equal(const entry_t* e, const void* key, int key_size, int hash) {
    return e->hash == hash && e->key_size == key_size && memcmp(&e->kv, key, key_size) == 0;     
}

static const entry_t* get_entry(const struct map_s* m, int hash, const void* key, int key_size) {
    const int ix = hash % m->capacity;
    const entry_t* s = m->entries[ix];
    const entry_t* e = s;
    while (e != null) {
        const entry_t* n = e->next;
        if (key_equal(e, key, key_size, hash)) {
            return e;
        }
        e = n == s ? null : n;
    }
    return null;
}

static void unlink_entry(struct map_s* m, int ix, entry_t* e) {
    if (e->next == e) {
        assert(m->entries[ix] == e && e->prev == e);
        m->entries[ix] = e->next = e->prev = null;
    } else {
        if (m->entries[ix] == e) { m->entries[ix] = e->next; }
        e->prev->next = e->next;
        e->next->prev = e->prev;
        e->next = e->prev = null;
    }
}    

static void link_entry(struct map_s* m, int ix, entry_t* e) {
    if (m->entries[ix] == null) {
        m->entries[ix] = e->next = e->prev = e;
    } else {
        e->next = m->entries[ix]->next;
        e->prev = m->entries[ix]->prev;
        m->entries[ix] = e;
        e->prev->next = e;
        e->next->prev = e;
    }
}    

static int resize_map(map_t map, int capacity) {
    struct map_s* m = *map;
    assert(MAP_MIN_CAPACITY <= capacity && m->size <= capacity * MAX_OCCUPANCY_PERCENTAGE / 100);
    int bytes = sizeof(struct map_s) + (capacity - 1) * sizeof(entry_t*);
    struct map_s* n = (struct map_s*)m->heap->allocate(m->heap,  bytes);
    if (n == null) { errno = ENOMEM; return -1; }
    memset(n, 0, bytes);
    n->capacity = capacity;
    n->size = m->size;
    n->modification_count = m->modification_count;
    n->heap = m->heap;
    for (int i = 0; i < m->capacity; i++) {
        while (m->entries[i] != null) {
            entry_t* e = m->entries[i];
            int ix = e->hash % capacity;
            unlink_entry(m, i, e);
            link_entry(n, ix, e);
        }
    }
    n->heap->deallocate(n->heap, m);
    *map = n; 
    return 0;
}

map_t map_create_with_heap(int capacity, map_heap_t* heap) {
    assert(capacity >= 4);
    map_t map = (map_t)heap->allocate(heap,  sizeof(map_t));
    if (map == null) { return null; }
    int bytes = sizeof(struct map_s) + (capacity - 1) * sizeof(entry_t*);
    *map = (struct map_s*)heap->allocate(heap,  bytes);
    if (*map == null) { heap->deallocate(heap, map); return null; }
    memset(*map, 0, bytes);
    (*map)->capacity = capacity;
    (*map)->heap = heap;
    return map;
}

static void* c_runtime_allocate(map_heap_t* heap, int bytes) { return malloc(bytes); }
static void* c_runtime_reallocate(map_heap_t* heap, void* a, int bytes) { return realloc(a, bytes); }
static void  c_runtime_deallocate(map_heap_t* heap, void* a) { free(a); }

static map_heap_t c_runtime_heap = {
    null,
    c_runtime_allocate,
    c_runtime_reallocate,
    c_runtime_deallocate
};

map_t map_create(int capacity) {
    return map_create_with_heap(capacity, &c_runtime_heap);
}

void map_destroy(map_t map) {
    if (map != null) {
        struct map_s* m = *map;
        map_heap_t* heap = m->heap;
        if (m != null) {
            for (int i = 0; i < m->capacity; i++) {
                entry_t* s = m->entries[i];
                entry_t* e = s;
                while (e != null) {
                    entry_t* n = e->next;
                    heap->deallocate(heap, e);
                    e = n == s ? null : n;                    
                }
                m->entries[i] = null;
            }
            heap->deallocate(heap, m);
        }
        heap->deallocate(heap, map);
    }
}

int  map_size(const map_t map) { return (*map)->size; }
int  map_capacity(const map_t map) { return (*map)->capacity; }

map_entry_t map_get(const map_t map, const void* key, int key_size) {
    struct map_s* m = *map;
    const int hash = map_hashcode(key, key_size);
    const entry_t* e = get_entry(m, hash, key, key_size);
    if (e == null) {
        map_entry_t r = { null }; // null, null, 0, 0
        return r;
    } else {
        map_entry_t r;
        r.key = &e->kv;
        r.val = &e->kv + e->key_size;
        r.key_size = key_size;
        r.val_size = e->val_size;
        return r;
    }
}

void map_remove(map_t map, const void* key, int key_size) {
    struct map_s* m = *map;
    int hash = map_hashcode(key, key_size);
    int ix = hash % m->capacity;
    entry_t* s = m->entries[ix];
    entry_t* e = s;
    while (e != null) {
        entry_t* n = e->next;
        if (key_equal(e, key, key_size, hash)) {
            unlink_entry(m, ix, e);
            m->size--;
            m->modification_count++;
            assert(m->size >= 0);            
            m->heap->deallocate(m->heap, e);
            e = null;
        } else {
            e = n == s ? null : n;
        }            
    }
    if (MAP_MIN_CAPACITY * 2 <= m->capacity && m->size * 2 < m->capacity * MAX_OCCUPANCY_PERCENTAGE / 100) {
        (void)resize_map(map, m->capacity / 2); // ignore shrinking failure on map_remove 
    }
}

int map_put(map_t map, const void* key, int key_size, const void* val, int val_size) {
    assert(key != null && key_size >= 1);
    assert(val != null && val_size >= 1);
    if ((*map)->size >= (*map)->capacity * MAX_OCCUPANCY_PERCENTAGE / 100) {
        if (resize_map(map, (*map)->capacity * 2) != 0) { return -1; }
    }
    struct map_s* m = *map;
    int hash = map_hashcode(key, key_size);
    int ix = hash % m->capacity;
    assert(m->size < m->capacity);
    entry_t* s = m->entries[ix];
    entry_t* e = s;
    while (e != null) {
        entry_t* x = e->next;
        if (key_equal(e, key, key_size, hash)) {
            if (e->val_size != val_size) {
                entry_t* kv = m->heap->allocate(m->heap,  sizeof(entry_t) - 1 + key_size + val_size);
                if (kv == null) { errno = ENOMEM; return -1; }
                memcpy(kv, &e, sizeof(entry_t) - 1 + key_size);
                memcpy(&(kv->kv) + key_size, val, val_size);
                e->prev->next = kv;                
                e->next->prev = kv;
                if (m->entries[ix] == e) { m->entries[ix] = kv; }                
                m->heap->deallocate(m->heap, e);
                assert(kv->hash == hash);
            } else {
                memcpy(&(e->kv) + key_size, val, val_size);
            }
            m->modification_count++;
            return 0;
        }
        e = x == s ? null : x;
    }
    entry_t* n = m->heap->allocate(m->heap,  sizeof(entry_t) - 1 + key_size + val_size);
    if (n == null) { errno = ENOMEM; return -1; }
    n->hash = hash;
    n->key_size = key_size;
    n->val_size = val_size;
    memcpy(&n->kv, key, key_size);
    memcpy(&(n->kv) + key_size, val, val_size);
    link_entry(m, ix, n);
    m->size++;
    assert(m->size < m->capacity);            
    m->modification_count++;
    return 0;
}

void map_iterate(const map_t map, void* that, map_iterator_t iterator) {
    struct map_s* m = *map;
    map_i64_t modification_count = m->modification_count;
    for (int i = 0; i < m->capacity; i++) {
        entry_t* s = m->entries[i];
        entry_t* e = s;
        while (e != null) {
            entry_t* n = e->next;
            map_entry_t r;
            r.key = &e->kv;
            r.val = &e->kv + e->key_size;
            r.key_size = e->key_size;
            r.val_size = e->val_size;
            iterator(that, &r);
            assert(m->modification_count == modification_count);
            e = n == s ? null : n;                    
        }
    }    
}

