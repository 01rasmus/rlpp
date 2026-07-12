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
                allocates a new entry of the pool
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

#ifndef RLPP_SWAP_BUFFER_SIZE
#define RLPP_SWAP_BUFFER_SIZE 256
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
    (&(POOL)[rlpp__header((POOL))->map_list[(uint32_t)(((ID) & 0xFFFFFFFF) - 1)].array_index])

#define rlpp_exists(POOL, ID) \
    (RLPP_FUNC(_get)((POOL), (ID)) != NULL)

#define rlpp_remove(POOL, ID) \
    RLPP_FUNC(_remove)((POOL), (ID))

#define rlpp_len(POOL) \
    ((POOL) ? (rlpp__header(POOL)->length) : 0)

#define rlpp_cap(POOL) \
    ((POOL) ? (rlpp__header(POOL)->capacity) : 0)

#define rlpp_sort(POOL, SORT_FUNCTION) \
    do { \
        _Static_assert(sizeof(*(POOL)) <= RLPP_SWAP_BUFFER_SIZE, "RLPP_SWAP_BUFFER_SIZE is too small for this pool type. define a larger value for the RLPP_SWAP_BUFFER_SIZE macro"); \
        RLPP_FUNC(_sort)((POOL), (SORT_FUNCTION)); \
    } while(0);

#define rlpp_free(POOL) \
    RLPP_FUNC(_free)((POOL))

#define rlpp_init_custom(POOL, CUSTOM_CAP, CUSTOM_ALLOC) \
    (POOL) = rlpp__header_to_pool(rlpp__init((CUSTOM_ALLOC), (CUSTOM_CAP), sizeof(*(POOL))))

#define rlpp__header(POOL) \
    ((rlpp_header_t*)(POOL) - 1)

#define rlpp__header_to_pool(HEADER) \
    ((void*)((rlpp_header_t*)(HEADER) + 1))

#define rlpp__maybe_grow(POOL, NUM) \
    (!(POOL) || ((rlpp_len((POOL)) + (NUM)) > rlpp_cap((POOL))) ? RLPP_FUNC(_grow)((void**)&(POOL), sizeof(*(POOL)), (NUM)) : 1)

#define rlpp__id_index(ID) \
    ((uint32_t)(((ID) & 0xFFFFFFFF) - 1))
    
#define rlpp__id_gen(ID) \
    ((uint32_t)(((ID) >> 32) & 0xFFFFFFFF))

typedef uint64_t rlpp_id_t;
typedef uint8_t rlpp_bool_t;

typedef int32_t (*rlpp_compare_callback_t)(const void* a, const void* b);

typedef struct rlpp_mapping_t {
    uint32_t generation;
    uint32_t array_index;
    uint32_t map_index;
    uint32_t child;
    rlpp_bool_t free;
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
    uint32_t next_free_map_index;
    uint32_t capacity;
    uint32_t length;
    uint32_t element_size;
    rlpp_bool_t size_is_fixed; //if this is true no reallocations will be made and the capacity won't change
} rlpp_header_t;

RLPPDEF rlpp_id_t RLPP_FUNC(_get_new_id)(void* pool);
RLPPDEF void RLPP_FUNC(_remove)(void* pool, rlpp_id_t id);
RLPPDEF rlpp_bool_t RLPP_FUNC(_grow)(void** pool, uint64_t element_size, uint32_t needed_entries);
RLPPDEF void RLPP_FUNC(_free)(void* pool);
RLPPDEF void RLPP_FUNC(_sort)(void* pool, rlpp_compare_callback_t sort_function);

static inline void* RLPP_FUNC(_get)(void* pool, rlpp_id_t id) {
    if(!pool || id == RLPP_NULL) {
        return NULL;
    }
    rlpp_header_t* header = rlpp__header(pool);
    uint32_t map_index = rlpp__id_index(id);
    uint32_t generation = rlpp__id_gen(id);
    if(map_index >= header->capacity) {
        return NULL;
    }
    rlpp_mapping_t* mapping = &header->map_list[map_index];
    if(mapping->free || mapping->generation != generation) {
        return NULL;
    }
    uint8_t* ptr = ((uint8_t*)pool) + mapping->array_index * header->element_size;
    return (void*)ptr;
}

static inline void* RLPP_FUNC(_get_fast)(void* pool, rlpp_id_t id) {
    rlpp_header_t* header = rlpp__header(pool);
    uint32_t map_index = rlpp__id_index(id);
    uint32_t generation = rlpp__id_gen(id);
    rlpp_mapping_t* mapping = &header->map_list[map_index];
    if(mapping->free || mapping->generation != generation) {
        return NULL;
    }
    uint8_t* ptr = ((uint8_t*)pool) + mapping->array_index * header->element_size;
    return (void*)ptr;
}

#endif /* RLPP_INCLUDE_H */

#ifdef RLPP_IMPLEMENTATION
#include <stddef.h>
#include <string.h>
#define RLPP_DEFAULT_CAPACITY           16
#define RLPP_QUICK_SORT_LEN_THRESHOLD   16

_Static_assert(RLPP_SWAP_BUFFER_SIZE > 0, "RLPP_SWAP_BUFFER_SIZE must be greated than 0");

typedef union rlpp__aligned_sort_buffer_t {
    max_align_t _;
    uint8_t bytes[RLPP_SWAP_BUFFER_SIZE];
} rlpp__aligned_sort_buffer_t;

static inline void* rlpp__default_alloc(uint64_t size, void* user) {
    return malloc(size);
}

static inline void rlpp__default_free(void* ptr, uint64_t size, void* user) {
    free(ptr);
}

static inline void* rlpp__default_realloc(void *old_pointer, uint64_t old_size, uint64_t new_size, void *user) {
    return realloc(old_pointer, new_size);
}

static inline void rlpp__initialize_new_map_list_entries(rlpp_header_t* header, rlpp_mapping_t* map_list, uint32_t start_index, uint32_t end_index) {
    for(uint32_t i = start_index; i < end_index; i++) {
        map_list[i] = (rlpp_mapping_t){
            .free = RLPP_TRUE,
            .generation = 0,
            .array_index = 0,
            .map_index = 0,
            .child = header->next_free_map_index,
        };
        header->next_free_map_index = i;
    }
}

static inline rlpp_header_t* rlpp__init(rlpp_allocator_t* custom_allocator, uint32_t custom_cap, uint64_t element_size) {
    rlpp_header_t* header = NULL;
    rlpp_mapping_t* map_list = NULL;

    static rlpp_allocator_t default_allocator = {
        .user = NULL,
        .alloc = &rlpp__default_alloc,
        .free = &rlpp__default_free,
        .realloc = &rlpp__default_realloc,
    };

    rlpp_allocator_t* allocator = custom_allocator ? custom_allocator : &default_allocator;
    uint64_t capacity = custom_cap == 0 ? RLPP_DEFAULT_CAPACITY : custom_cap;
    uint64_t size = sizeof(rlpp_header_t) + (capacity * element_size);
    uint64_t map_size = sizeof(rlpp_mapping_t) * capacity;

    header = allocator->alloc(size, allocator->user);
    if(!header) {
        goto err;
    }
    map_list = allocator->alloc(map_size, allocator->user);
    if(!map_list) {
        goto err;
    }

    header->allocator = (*allocator);
    header->capacity = capacity;
    header->size_is_fixed = custom_cap != 0;
    header->length = 0;
    header->map_list = map_list;
    header->element_size = element_size;
    header->next_free_map_index = UINT32_MAX;
    rlpp__initialize_new_map_list_entries(header, map_list, 0, capacity);
    return header;
err:
    if(header) {
        allocator->free(header, size, allocator->user);
    }
    if(map_list) {
        allocator->free(map_list, map_size, allocator->user);
    }
    return NULL;
}

static inline rlpp_header_t* rlpp__resize(rlpp_header_t* header, uint32_t new_cap) {
    uint32_t old_cap = header->capacity;
    uint64_t old_size = sizeof(rlpp_header_t) + header->element_size * old_cap;
    uint64_t new_size = sizeof(rlpp_header_t) + header->element_size * new_cap;
    uint64_t old_mapping_size = sizeof(rlpp_mapping_t) * old_cap;
    uint64_t new_mapping_size = sizeof(rlpp_mapping_t) * new_cap;
    rlpp_mapping_t* new_map_list = header->allocator.realloc(header->map_list, old_mapping_size, new_mapping_size, header->allocator.user);
    if(!new_map_list) {
        return NULL;
    }
    header->map_list = new_map_list;
    rlpp_header_t* new_header = header->allocator.realloc(header, old_size, new_size, header->allocator.user);
    if(!new_header) {
        return NULL;
    }
    
    new_header->capacity = new_cap;
    rlpp__initialize_new_map_list_entries(new_header, new_map_list, old_cap, new_cap);
    return new_header;
}

RLPPDEF rlpp_id_t RLPP_FUNC(_get_new_id)(void* pool) {
    rlpp_header_t* header = rlpp__header(pool);

    uint32_t new_map_index = header->next_free_map_index;
    rlpp_mapping_t* mapping = &header->map_list[new_map_index];
    if(mapping->free == RLPP_FALSE) {
        return RLPP_NULL;
    }
    uint32_t new_array_index = header->length - 1;
    header->next_free_map_index = mapping->child;
    header->map_list[new_array_index].map_index = new_map_index;
    mapping->child = 0;
    mapping->generation++;
    mapping->array_index = new_array_index;
    mapping->free = RLPP_FALSE;
    return ((uint64_t)(mapping->generation) << 32 | ((uint64_t)(new_map_index) + 1));
}

RLPPDEF void RLPP_FUNC(_remove)(void* pool, rlpp_id_t id) {
    if(!pool) {
        return;
    }
    rlpp_header_t* header = rlpp__header(pool);
    uint32_t map_index = rlpp__id_index(id);
    uint32_t gen = rlpp__id_gen(id);
    if(map_index >= header->capacity) {
        return;
    }

    rlpp_mapping_t* mapping = &header->map_list[map_index];
    if(mapping->free || mapping->generation != gen) {
        return;
    }
    uint32_t last_array_index = header->length - 1;
    mapping->free = RLPP_TRUE;
    mapping->child = header->next_free_map_index;
    header->next_free_map_index = map_index;
    header->length--;
    if(mapping->array_index == last_array_index) {
        return;
    }
    memcpy((uint8_t*)pool + (mapping->array_index * header->element_size), (uint8_t*)pool + (last_array_index * header->element_size), header->element_size);
    uint32_t swap_map_index = header->map_list[last_array_index].map_index;
    rlpp_mapping_t* swap_map = &header->map_list[swap_map_index];
    swap_map->array_index = mapping->array_index;
}

RLPPDEF rlpp_bool_t RLPP_FUNC(_grow)(void** pool_ptr, uint64_t element_size, uint32_t needed_entries) {
    void* pool = *pool_ptr;
    if(pool ? rlpp__header(pool)->size_is_fixed : 0) {
        return RLPP_FALSE;
    }

    uint32_t new_capacity = pool ? rlpp__header(pool)->capacity : RLPP_DEFAULT_CAPACITY;
    uint32_t new_length = pool ? (rlpp__header(pool)->length + needed_entries) : needed_entries;
    while(new_length > new_capacity) {
        new_capacity *= 2;
    }

    if(!pool) {
        rlpp_header_t* new_header = rlpp__init(NULL, 0, element_size);
        if(!new_header) {
            return RLPP_FALSE;
        }
        (*pool_ptr) = rlpp__header_to_pool(new_header);
    } else {
        rlpp_header_t* new_header = rlpp__resize(rlpp__header(pool), new_capacity);
        if(!new_header) {
            return RLPP_FALSE;
        }
        (*pool_ptr) = rlpp__header_to_pool(new_header);
    }

    return RLPP_TRUE;
}

static inline void rlpp__swap_array_indices(uint8_t* data, uint32_t a_index, uint32_t b_index) {
    rlpp__aligned_sort_buffer_t temp;
    rlpp_header_t* header = rlpp__header(data);

    uint32_t a_id = header->map_list[a_index].map_index;
    uint32_t b_id = header->map_list[b_index].map_index;

    memcpy(temp.bytes, data + a_index * header->element_size, header->element_size);
    memcpy(data + a_index * header->element_size, data + b_index * header->element_size, header->element_size);
    memcpy(data + b_index * header->element_size, temp.bytes, header->element_size);

    header->map_list[a_index].map_index = b_id;
    header->map_list[b_index].map_index = a_id;
    header->map_list[a_id].array_index = b_index;
    header->map_list[b_id].array_index = a_index;
}

static void rlpp__insertion_sort(rlpp_header_t* header, rlpp_compare_callback_t sort_function, uint8_t* data) {
    for(uint32_t i = 1; i < header->length; i++) {
        uint32_t j = i;

        while(j > 0) {
            void* prev = data + (size_t)(j - 1) * header->element_size;
            void* curr = data + (size_t)(j) * header->element_size;

            if(sort_function(prev, curr) <= 0) {
                break;
            }

            rlpp__swap_array_indices(data, j - 1, j);
            j--;
        }
    }
}

static inline void* rlpp__median_of_three(void* a, void* b, void* c, rlpp_compare_callback_t sort_function) {
    if(sort_function(a, b) < 0) {
        if(sort_function(b, c) < 0) {
            return b;
        }
        if(sort_function(a, c) < 0) {
            return c;
        }
        return a;
    }

    if(sort_function(a, c) < 0) {
        return a;
    }
    if(sort_function(b, c) < 0) {
        return c;
    }
    return b;
}

static inline int64_t rlpp__quick_sort_partition(rlpp_header_t* header, rlpp_compare_callback_t sort_function, uint8_t* data, int64_t low, int64_t high) {
    int64_t middle = (low + (high - low) / 2);
    uint8_t* a = data + low * header->element_size;
    uint8_t* b = data + middle * header->element_size;
    uint8_t* c = data + high * header->element_size;

    rlpp__aligned_sort_buffer_t pivot;
    memcpy(pivot.bytes, rlpp__median_of_three(a, b, c, sort_function), header->element_size);

    int64_t i = low - 1;

    for(int64_t j = low; j <= high - 1; j++) {
        uint8_t* curr = data + header->element_size * j;
        if(sort_function(curr, pivot.bytes) <= 0) {
            i++;
            rlpp__swap_array_indices(data, i, j);
        }
    }

    rlpp__swap_array_indices(data, i + 1, high);
    return i + 1;
}

static void rlpp__quick_sort(rlpp_header_t* header, rlpp_compare_callback_t sort_function, uint8_t* data, int64_t low, int64_t high) {
    if(low < high) {
        int64_t partition_index = rlpp__quick_sort_partition(header, sort_function, data, low, high);
        rlpp__quick_sort(header, sort_function, data, low, partition_index - 1);
        rlpp__quick_sort(header, sort_function, data, partition_index + 1, high);
    }
}

RLPPDEF void RLPP_FUNC(_sort)(void* pool, rlpp_compare_callback_t sort_function) {
    if(!pool) {
        return;
    }

    rlpp_header_t* header = rlpp__header(pool);
    uint8_t* data = pool;

    if(header->length == 0) {
        return;
    }

    if(header->length <= RLPP_QUICK_SORT_LEN_THRESHOLD) {
        rlpp__insertion_sort(header, sort_function, data);
    } else {
        rlpp__quick_sort(header, sort_function, data, 0, header->length - 1);
    }
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