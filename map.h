#pragma once
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

#include <strings.h> // for strlen()

/* Map implements simple hash table with trivial hashcode function and double linked
   list collision resolution. It was initially implemented as a part of Modula-2 
   compiler for Kronos workstation in 1983-1984. Original code can be found at:
   https://github.com/leok7v/kronos/blob/master/excelsior/src/sys/util/mx/mxScan.m
   This is result of a weekend project re-implementing it in C.  
   Limitations: keys and values should not be null and should have >= 1 byte size.
   2GiB - 1 is maximum key or value size.
   Map stores copies of kays and values.
   Map attempts to double its capacity when occupancy rate > 61%.
   Map attempts to shrink in half when occupancy rate < 25%.
   Map resizing does not move key-value pair in memory - thus calling code
   can safely keep pointers to values over new items insertions. 
   There is no synchronization and thus map is not thread safe.
   Map uses double linked lists to resolve collisions and thus 
   have exta 16 (for 64 bit systems) or 8 (for 32 bit systems) bytes overhead. 
   Actual overhead is 16 + 12 = 28 or 8 + 12 = 20 bytes per each key-value pair.
*/

enum { 
    MAP_MIN_CAPACITY = 4 // less then 4 makes little sense and thus prohibited
}; 

struct map_s;

typedef struct map_s **map_t;

struct map_heap_s;
typedef struct map_heap_s map_heap_t;

typedef struct map_heap_s {
    void* that; // not striktly necessary but simplifies heap struct layout
    void* (*allocate)(map_heap_t* heap, int bytes);
    void* (*reallocate)(map_heap_t* heap, void* address, int bytes);
    void  (*deallocate)(map_heap_t* heap, void* address);
} map_heap_t;

map_t map_create(int capacity);
map_t map_create_with_heap(int capacity, map_heap_t* heap);
int map_size(const map_t map);
int map_capacity(const map_t map);
int map_hashcode(const void* key, const int key_size); // always > 0
int map_put(map_t map, const void* key, int key_size, const void* val, int val_size);
/* map is not modified if map_put() fails (returns result != 0 it sets errno to ENOMEM) */
void map_remove(map_t map, const void* key, int key_size);
void map_destroy(map_t map);

typedef struct map_entry_s {
    const void* key;
    const void* val;
    int key_size;
    int val_size;
} map_entry_t;

map_entry_t map_get(const map_t map, const void* key, int key_size);

typedef void (*map_iterator_t)(void* that, const map_entry_t* e);

void map_iterate(const map_t map, void* that, map_iterator_t iterator);

typedef long long map_i64_t;

inline const char* map_get_str_str(const map_t map, const char* key) { return (const char*)(map_get(map, key, (int)strlen(key) + 1).val); }
inline void map_put_str_str(const map_t map, const char* key, const char* val) { map_put(map, key, (int)strlen(key) + 1, val, (int)strlen(val) + 1); }

inline const map_i64_t* map_get_str_i64(const map_t map, const char* key) { return (const map_i64_t*)(map_get(map, key, (int)strlen(key) + 1).val); }
inline void map_put_str_i64(const map_t map, const char* key, map_i64_t val) { map_put(map, key, (int)strlen(key) + 1, &val, (int)sizeof(val)); }

inline const char* map_get_i64_str(const map_t map, map_i64_t key) { return (const char*)(map_get(map, &key, sizeof(key)).val); }
inline void map_put_i64_str(const map_t map, map_i64_t key, const char* val) { map_put(map, &key, (int)sizeof(key), val, (int)strlen(val) + 1); }

inline const map_i64_t* map_get_i64_i64(const map_t map, map_i64_t key) { return (const map_i64_t*)(map_get(map, &key, (int)sizeof(key)).val); }
inline void map_put_i64_i64(const map_t map, map_i64_t key, map_i64_t val) { map_put(map, &key, (int)sizeof(key), &val, (int)sizeof(val)); }

inline const double* map_get_str_d(const map_t map, const char* key) { return (const double*)(map_get(map, key, (int)strlen(key) + 1).val); }
inline void map_put_str_d(const map_t map, const char* key, double val) { map_put(map, key, (int)strlen(key) + 1, &val, (int)sizeof(val)); }

/* Note that: uintptr_t is castable to "long long"; more polymorphic accessors can be added to map_get/put if needed */
