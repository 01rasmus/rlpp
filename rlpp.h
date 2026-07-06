/*
    RL Packed Pool
        This single header file library implements
        a data structure where allocations and removals
        always ensures that the data is packed tightly.

        This means that raw pointers cannot be used when
        referencing data to the POOL. Therefore an allocation
        returns a handle that is stable until it is freed, and
        can be used to map into the real POOL data.

    DOCUMENTATION

        To declare a new empty pool of type T

            T* pool = NULL;

        Functions

            rlpp_id_t rlpp_alloc(T* pool, T value);
                allocates a new entry of the POOL
                and returns a new stable id that points
                to the new data. if a 0 or RLPP_NULL id
                is returned, an allocation failed
                (out of memory)

            void* rlpp_get(T* pool, rlpp_id_t id);
                returns the pointer to the data
                inside the pool given the stable id
                receieved by the alloc function

            T* rlpp_get_unchecked(T* pool, rlpp_id_t id);
                same as above but faster because
                it doesnt do any checks. this should only
                be used whenever the id is sure to be valid.
                either validate it with the rlpp_get function
                and see if it returns a non-null pointer or
                use the rlpp_exists function

            bool rlpp_exists(T* pool, rlpp_id_t id);
                returns true if the id points to a
                valid value inside the pool. if the id
                or the pool is null it returns false.
                if the generation of the current value
                at a given index doesnt match the one
                from the id, this returns false.
                if the mapped value is free, it returns
                false since the value has been freed.

            void rlpp_remove(T* pool, rlpp_id_t id);
                removes the data that is behind the
                given id inside the pool. the data
                at the end gets swapped to ensure
                the data is packed

            uint32_t rlpp_len(T* pool);
                returns the amount of entries
                inside the pool

            void rlpp_free(T* pool);
                frees the entire pool
                (should not be confused with
                the remove function)

    COMPILE-TIME OPTIONS
        #define RLPP_IMPLEMENTATION
            needs to be used in one C file
            before including this library

        #define RLPP_STATIC
            adds static too all the functions

        #define RLPP_PREFIX
            adds a prefix to all the functions
            to avoid collisions if this lib
            is used multiple times in the same
            project
*/

#ifndef RLPP_INCLUDE_H
#define RLPP_INCLUDE_H

#include <stdlib.h>
#include <stdint.h>

#ifndef RLPPDEF
#ifdef RLPP_STATIC
#define RLPPDEF static
#else
#define RLPPDEF extern
#endif
#endif

#ifndef RLPP_PREFIX
#define RLPP_PREFIX rlpp_
#endif

#define RLPP_TRUE           (1)
#define RLPP_FALSE          (0)
#define RLPP_NULL           ((rlpp_id_t)0)

#define rlpp_alloc(POOL, VALUE) \
    ((rlpp__maybe_grow((POOL), 1)) ? ((POOL)[rlpp__header(POOL)->length++] = (VALUE), rlpp__get_new_id((POOL))) : RLPP_NULL)

#define rlpp_exists(POOL, ID) \
    (rlr_get(POOL, ID) != NULL)

#define rlpp_remove(POOL, ID) \
    rlpp__remove((POOL), (ID))

#define rlpp_len(POOL) \
    ((POOL) ? (rlpp__header(POOL)->length) : 0)

#define rlpp_cap(POOL) \
    ((POOL) ? (rlpp__header(POOL)->capacity) : 0)

#define rlpp_free(POOL) \
    ((POOL) ? (free(rlpp__header((POOL))->map_list), free(rlpp__header((POOL))), 0) : (0))

#define rlpp__header(POOL) \
    ((rlpp_header_t*)(POOL) - 1)

#define rlpp__header_to_pool(HEADER) \
    ((void*)((rlpp_header_t*)(HEADER) + 1))

#define rlpp__maybe_grow(POOL, NUM) \
    (!(POOL) || ((rlpp_len((POOL)) + (NUM)) > rlpp_cap((POOL))) ? rlpp__grow((void**)&(POOL), sizeof(*(POOL)), (NUM)) : 1)

typedef uint64_t rlpp_id_t;

typedef struct rlpp_mapping_t {
    uint32_t generation;
    uint32_t index; //this index goes into the actual pool array
    uint8_t free;
} rlpp_mapping_t;

typedef struct rlpp_header_t {
    rlpp_mapping_t* map_list;
    uint32_t capacity;
    uint32_t length;
    uint32_t element_size;
} rlpp_header_t;

RLPPDEF rlpp_id_t rlpp__get_new_id(void* pool);
RLPPDEF void rlpp__remove(void* pool, rlpp_id_t id);
RLPPDEF uint8_t rlpp__grow(void** pool, uint64_t element_size, uint32_t needed_entries);

static inline void* rlpp_get(void* pool, rlpp_id_t id) {
    if(!pool || id == RLPP_NULL) {
        return NULL;
    }
    rlpp_header_t* header = rlpp__header(pool);
    uint32_t map_index = (uint32_t)((id & 0xFFFFFFFF) - 1);
    uint32_t generation = (uint32_t)((id >> 32) & 0xFFFFFFFF);
    if(map_index >= header->capacity) {
        return NULL;
    }
    rlpp_mapping_t* mapping = &header->map_list[map_index];
    if(mapping->free || mapping->generation != generation) {
        return NULL;
    }
    uint8_t* ptr = ((uint8_t*)pool) + mapping->index * header->element_size;
    return (void*)ptr;
}

#define rlpp_get_unchecked(POOL, ID) \
    (&(POOL)[rlpp__header(POOL)->map_list[(uint32_t)(((ID) & 0xFFFFFFFF) - 1)].index])

#endif /* RLPP_INCLUDE_H */

#ifdef RLPP_IMPLEMENTATION
#define RLPP_DEFAULT_CAPACITY   16

RLPPDEF rlpp_id_t rlpp__get_new_id(void* pool) {
    rlpp_header_t* header = rlpp__header(pool);
    uint32_t last_index = header->length - 1;

    for(uint64_t i = 0; i < header->capacity; i++) {
        rlpp_mapping_t* map = &header->map_list[i];
        
        if(map->free) {
            map->generation++;
            map->index = last_index;
            map->free = RLPP_FALSE;
            return ((uint64_t)(map->generation) << 32 | ((uint64_t)(i) + 1));
        }
    }

    return RLPP_NULL;
}

RLPPDEF void rlpp__remove(void* pool, rlpp_id_t id) {
    if(!pool) {
        return;
    }
    rlpp_header_t* header = rlpp__header(pool);

    //todo:
}

RLPPDEF uint8_t rlpp__grow(void** pool_ptr, uint64_t element_size, uint32_t needed_entries) {
    void* pool = *pool_ptr;
    uint32_t new_capacity = pool ? rlpp__header(pool)->capacity : RLPP_DEFAULT_CAPACITY;
    uint32_t new_length = pool ? (rlpp__header(pool)->length + needed_entries) : needed_entries;
    while(new_length > new_capacity) {
        new_capacity *= 2;
    }

    uint64_t total_size = sizeof(rlpp_header_t) + (element_size * new_capacity);
    uint64_t mapping_size = new_capacity * sizeof(rlpp_mapping_t);

    if(!pool) {
        rlpp_header_t* new_header = malloc(total_size);
        if(!new_header) {
            return RLPP_FALSE;
        }
        new_header->map_list = malloc(mapping_size);
        if(!new_header->map_list) {
            free(new_header);
            return RLPP_FALSE;
        }
        for(uint64_t i = 0; i < new_capacity; i++) {
            new_header->map_list[i] = (rlpp_mapping_t) {
                .free = RLPP_TRUE,
                .generation = 0,
                .index = 0,
            };
        }

        new_header->capacity = new_capacity;
        new_header->length = 0;
        new_header->element_size = element_size;
        (*pool_ptr) = rlpp__header_to_pool(new_header);
    } else {
        rlpp_mapping_t* new_map_list = realloc(rlpp__header(pool)->map_list, mapping_size);
        if(!new_map_list) {
            return RLPP_FALSE;
        }
        rlpp__header(pool)->map_list = new_map_list;

        rlpp_header_t* new_header = realloc(rlpp__header(pool), total_size);
        if(!new_header) {
            return RLPP_FALSE;
        }
        
        for(uint64_t i = new_header->capacity; i < new_capacity; i++) {
            new_header->map_list[i] = (rlpp_mapping_t) {
                .free = RLPP_TRUE,
                .generation = 0,
                .index = 0,
            };
        }
        new_header->capacity = new_capacity;
        (*pool_ptr) = rlpp__header_to_pool(new_header);
    }

    return RLPP_TRUE;
}

#endif /* RLPP_IMPLEMENTATION */

/*
    MIT License

    Copyright (c) 2026 Rasmus

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/