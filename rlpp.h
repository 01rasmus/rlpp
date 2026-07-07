/*
    rlpp.h - RL Packed Pool
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
            
            void* rlpp_get_fast(T* pool, rlpp_id_t id);
                same as rlpp_get but faster due to less
                bounds and pointer checks

            T* rlpp_get_unchecked(T* pool, rlpp_id_t id);
                same as rlpp_get but faster because
                it doesnt do any checks. this should only
                be used whenever the id is sure to be valid.
                either validate it with the rlpp_get function
                and see if it returns a non-null pointer or
                use the rlpp_exists function

            uint8_t rlpp_exists(T* pool, rlpp_id_t id);
                returns non-zero if the id points to a
                valid value inside the pool. if the id
                or the pool is null it returns 0.
                if the generation of the current value
                at a given index doesnt match the one
                from the id, this returns 0.
                if the mapped value is free, it returns
                0 since the value has been freed.

            void rlpp_remove(T* pool, rlpp_id_t id);
                removes the data that is behind the
                given id inside the pool. the data
                at the end gets swapped to ensure
                the data is packed

            uint32_t rlpp_len(T* pool);
                returns the amount of entries
                inside the pool

            uint32_t rlpp_cap(T* pool);
                returns the capacity of the pool

            void rlpp_free(T* pool);
                frees the entire pool
                (should not be confused with
                the remove function)

            void rlpp_init_custom(T* pool, uint64_t capacity, rlpp_allocator_t* custom_allocator);
                this can be used to optionally
                initialize the pool with a custom
                allocator or a capacity.
                if the capacity is non-zero it will
                not issue any reallocations. if it is 0,
                it will grow as needed with the realloc
                function, in the allocator. a custom
                allocator can be attached but if NULL
                is provided, a default allocator using
                the c lib's default, malloc, realloc and free
                functions.

                returns RLPP_FALSE when it fails and RLPP_TRUE
                when it succeeds. the pool should not be used if
                it returns RLPP_FALSE, because then the pool
                stays null, and may use the default allocator

                note: 'rlp_init_custom(pool, 0, NULL)' will
                      therefore make the pool behave as
                      default

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
            (the macro names are not affected)
*/

#ifndef RLPP_INCLUDE_H
#define RLPP_INCLUDE_H

#include <stdlib.h>
#include <stdint.h>

#ifndef RLPPDEF
#ifdef RLPP_STATIC
#define RLPPDEF static
#else
#define RLPPDEF
#endif
#endif

#ifndef RLPP_PREFIX
#define RLPP_PREFIX rlpp_
#endif

#define RLPP_CONCAT(PREFIX, NAME)               PREFIX##NAME
#define RLPP_FUNC_WITH_PREFIX(PREFIX, NAME)     RLPP_CONCAT(PREFIX, NAME)
#define RLPP_FUNC(NAME)                         RLPP_FUNC_WITH_PREFIX(RLPP_PREFIX, NAME)

#define RLPP_TRUE           (1)
#define RLPP_FALSE          (0)
#define RLPP_NULL           ((rlpp_id_t)0)

#define rlpp_alloc(POOL, VALUE) \
    ((rlpp__maybe_grow((POOL), 1)) ? ((POOL)[rlpp__header(POOL)->length++] = (VALUE), RLPP_FUNC(_get_new_id)((POOL))) : RLPP_NULL)

#define rlpp_get(POOL, ID) \
    RLPP_FUNC(_get)((POOL), (ID))

#define rlpp_get_fast(POOL, ID) \
    RLPP_FUNC(_get_fast)((POOL), (ID))

#define rlpp_get_unchecked(POOL, ID) \
    (&(POOL)[rlpp__header((POOL))->map_list[(uint32_t)(((ID) & 0xFFFFFFFF) - 1)].index])

#define rlpp_exists(POOL, ID) \
    (RLPP_FUNC(_get)((POOL), (ID)) != NULL)

#define rlpp_remove(POOL, ID) \
    RLPP_FUNC(_remove)((POOL), (ID))

#define rlpp_len(POOL) \
    ((POOL) ? (rlpp__header(POOL)->length) : 0)

#define rlpp_cap(POOL) \
    ((POOL) ? (rlpp__header(POOL)->capacity) : 0)

#define rlpp_free(POOL) \
    RLPP_FUNC(_free)((POOL))

#define rlpp_init_custom(POOL, CAPACITY, ALLOCATOR) \
    RLPP_FUNC(_init_custom)((void**)&(POOL), sizeof(*(POOL)), (CAPACITY), (ALLOCATOR))

#define rlpp__header(POOL) \
    ((rlpp_header_t*)(POOL) - 1)

#define rlpp__header_to_pool(HEADER) \
    ((void*)((rlpp_header_t*)(HEADER) + 1))

#define rlpp__maybe_grow(POOL, NUM) \
    (!(POOL) || ((rlpp_len((POOL)) + (NUM)) > rlpp_cap((POOL))) ? RLPP_FUNC(_grow)((void**)&(POOL), sizeof(*(POOL)), (NUM)) : 1)

typedef uint64_t rlpp_id_t;

typedef struct rlpp_mapping_t {
    uint32_t generation;
    uint32_t index; //this index goes into the actual pool array
    uint8_t free;
} rlpp_mapping_t;

typedef struct rlpp_allocator_t {
    void* (*alloc)(uint64_t size, void* user);
    void* (*realloc)(void* old_pointer, uint64_t old_size, uint64_t new_size, void* user);
    void (*free)(void* pointer, uint64_t size, void* user);
    void* user;
} rlpp_allocator_t;

typedef struct rlpp_header_t {
    rlpp_allocator_t allocator;
    rlpp_mapping_t* map_list;
    uint32_t capacity;
    uint32_t length;
    uint32_t element_size;
    uint8_t size_is_fixed; //if this is true no reallocations will be made and the capacity won't change
} rlpp_header_t;

RLPPDEF rlpp_id_t RLPP_FUNC(_get_new_id)(void* pool);
RLPPDEF void RLPP_FUNC(_remove)(void* pool, rlpp_id_t id);
RLPPDEF uint8_t RLPP_FUNC(_grow)(void** pool, uint64_t element_size, uint32_t needed_entries);
RLPPDEF uint8_t RLPP_FUNC(_init_custom)(void** pool, uint64_t element_size, uint64_t capacity, rlpp_allocator_t* allocator);
RLPPDEF void RLPP_FUNC(_free)(void* pool);

static inline void* RLPP_FUNC(_get)(void* pool, rlpp_id_t id) {
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

static inline void* RLPP_FUNC(_get_fast)(void* pool, rlpp_id_t id) {
    if(id == RLPP_NULL) {
        return NULL;
    }
    rlpp_header_t* header = rlpp__header(pool);
    uint32_t map_index = (uint32_t)((id & 0xFFFFFFFF) - 1);
    uint32_t generation = (uint32_t)((id >> 32) & 0xFFFFFFFF);
    rlpp_mapping_t* mapping = &header->map_list[map_index];
    if(mapping->free || mapping->generation != generation) {
        return NULL;
    }
    uint8_t* ptr = ((uint8_t*)pool) + mapping->index * header->element_size;
    return (void*)ptr;
}

#endif /* RLPP_INCLUDE_H */

#ifdef RLPP_IMPLEMENTATION
#include <string.h>
#define RLPP_DEFAULT_CAPACITY   16

static inline void* RLPP_FUNC(_default_alloc)(uint64_t size, void* user) {
    return malloc(size);
}

static inline void RLPP_FUNC(_default_free)(void* ptr, uint64_t size, void* user) {
    free(ptr);
}

static inline void* RLPP_FUNC(_default_realloc)(void *old_pointer, uint64_t old_size, uint64_t new_size, void *user) {
    return realloc(old_pointer, new_size);
}

static rlpp_allocator_t RLPP_FUNC(_default_allocator) = {
    .user = NULL,
    .alloc = &RLPP_FUNC(_default_alloc),
    .free = &RLPP_FUNC(_default_free),
    .realloc = &RLPP_FUNC(_default_realloc),
};

RLPPDEF rlpp_id_t RLPP_FUNC(_get_new_id)(void* pool) {
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

RLPPDEF void RLPP_FUNC(_remove)(void* pool, rlpp_id_t id) {
    if(!pool) {
        return;
    }
    rlpp_header_t* header = rlpp__header(pool);

    //todo:
}

RLPPDEF uint8_t RLPP_FUNC(_grow)(void** pool_ptr, uint64_t element_size, uint32_t needed_entries) {
    void* pool = *pool_ptr;
    if(pool ? rlpp__header(pool)->size_is_fixed : 0) {
        return RLPP_FALSE;
    }

    uint32_t new_capacity = pool ? rlpp__header(pool)->capacity : RLPP_DEFAULT_CAPACITY;
    uint32_t new_length = pool ? (rlpp__header(pool)->length + needed_entries) : needed_entries;
    while(new_length > new_capacity) {
        new_capacity *= 2;
    }

    uint64_t total_size = sizeof(rlpp_header_t) + (element_size * new_capacity);
    uint64_t mapping_size = new_capacity * sizeof(rlpp_mapping_t);

    if(!pool) {
        rlpp_header_t* new_header = RLPP_FUNC(_default_allocator).alloc(total_size, NULL);
        if(!new_header) {
            return RLPP_FALSE;
        }
        new_header->map_list = RLPP_FUNC(_default_allocator).alloc(mapping_size, NULL);
        if(!new_header->map_list) {
            RLPP_FUNC(_default_allocator).free(new_header, total_size, NULL);
            return RLPP_FALSE;
        }
        for(uint64_t i = 0; i < new_capacity; i++) {
            new_header->map_list[i] = (rlpp_mapping_t) {
                .free = RLPP_TRUE,
                .generation = 0,
                .index = 0,
            };
        }

        memcpy(&new_header->allocator, &RLPP_FUNC(_default_allocator), sizeof(rlpp_allocator_t));
        new_header->capacity = new_capacity;
        new_header->length = 0;
        new_header->element_size = element_size;
        new_header->size_is_fixed = RLPP_FALSE;
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

RLPPDEF uint8_t RLPP_FUNC(_init_custom)(void** pool, uint64_t element_size, uint64_t custom_capacity, rlpp_allocator_t* custom_allocator) {
    rlpp_allocator_t* allocator = custom_allocator ? custom_allocator : &RLPP_FUNC(_default_allocator);
    rlpp_header_t* header = NULL;
    rlpp_mapping_t* mapping = NULL;
    uint64_t capacity = custom_capacity == 0 ? RLPP_DEFAULT_CAPACITY : custom_capacity;
    uint64_t header_allocation_size = sizeof(rlpp_header_t) + (capacity * element_size);
    uint64_t mapping_allocation_size = sizeof(rlpp_mapping_t) * capacity;

    header = allocator->alloc(header_allocation_size, allocator->user);
    if(!header) {
        goto err;
    }
    mapping = allocator->alloc(mapping_allocation_size, allocator->user);
    if(!mapping) {
        goto err;
    }

    memcpy(&header->allocator, allocator, sizeof(rlpp_allocator_t));
    header->capacity = capacity;
    header->size_is_fixed = custom_capacity != 0;
    header->length = 0;
    header->map_list = mapping;
    header->element_size = element_size;
    
    for(uint64_t i = 0; i < capacity; i++) {
        header->map_list[i] = (rlpp_mapping_t) {
            .free = RLPP_TRUE,
            .generation = 0,
            .index = 0,
        };
    }

    (*pool) = rlpp__header_to_pool(header);
    return RLPP_TRUE;
err:
    if(header) {
        allocator->free(header, header_allocation_size, allocator->user);
    }
    if(mapping) {
        allocator->free(mapping, mapping_allocation_size, allocator->user);
    }
    (*pool) = NULL;
    return RLPP_FALSE;
}

RLPPDEF void RLPP_FUNC(_free)(void* pool) {
    if(!pool) {
        return;
    }
    rlpp_header_t* header = rlpp__header(pool);
    header->allocator.free(header->map_list, header->capacity * sizeof(rlpp_mapping_t), header->allocator.user);
    header->allocator.free(header, sizeof(rlpp_header_t) + (header->element_size * header->capacity), header->allocator.user);
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