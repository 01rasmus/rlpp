# 📦 rlpp - RL Packed Pool
A tightly packed data structure accessible with stable id handles written with cache friendliness in mind. It is a single header file library written in C.

## How to Use
In one source file, include the header file with the implementation define to compile the function definitions.
```C
#define RLPP_IMPLEMENTATION
#include <rlpp.h>
```
Then in any other source files, include it normally.
```C
#include <rlpp.h>
```
There are also two ways to combat symbol collision when using this library.
```C
/*
    make functions static. this only
    needs to be declared together with
    the RLPP_IMPLEMENTATION define
*/
#define RLPP_STATIC

/*
    if the library needs to be used
    from multiple translation units,
    a prefix can be used instead.
    this however, needs to be declared
    in every file that includes rlpp.h
*/
#define RLPP_PREFIX custom_prefix
```

## Basic Examples
To create a new pool, simply set a pointer type to null. Any mutation to the pool will automatically allocate and set the pointer behind the scenes.
```C
typedef struct my_type_t {
    int a;
} my_type_t;

int main() {
    my_type_t* pool = NULL;
    return 0;
}
```
To allocate a new entry to the pool do the following. The id that is returned is guaranteed to be stable throughout it's lifetime. If the allocation fails, the returned id will be equal to RLPP_NULL.
```C
my_type_t new_entry = {
    .a = 10,
};

rlpp_id_t id = rlpp_alloc(pool, new_entry);
```
There are three ways to fetch an entry's pointer given it's id.
```C
/*
    this is the most safe, but also the slowest since it 
    does the most checks. if the pool is NULL or the id
    is RLPP_NULL, it will return NULL
*/
my_type_t* normal = rlpp_get(pool, id);

/*
    this is a little less safe but faster since
    it doesn't check if the pool is NULL or
    if the id is RLPP_NULL
*/
my_type_t* fast = rlpp_get_fast(pool, id);

/*
    this is the absolute fastest but is unsafe.
    invalid inputs will bring undefined behaviour.

    this should only be used when the id is
    sure to be valid.

    caution: an id consist of an index slot part
    and a generation part. the unchecked fetch may
    use ids that have an old generation for a specific
    slot and return the pointer that is at that location
    while the others will return NULL in this case
    since the id would be outdated in that case
*/
my_type_t* unchecked = rlpp_get_unchecked(pool, id);
```

To remove an entry do the following. This will add the id's slot index to a free list internally and will be reused with future allocations, but with a higher generation number. Entries will be swapped when a removal occurs to ensure the array is contiguous in memory.
```C
rlpp_remove(pool, id);
```
Since the array is contiguous it is very easy to loop through the entries. Keep in mind that the index of the array is not the same as the id that is returned from the rlpp_alloc function.
```C
for(uint32_t i = 0; i < rlpp_len(pool); i++) {
    my_type_t* entry = &pool[i];
}
```
To free the pool do the following.
```C
rlpp_free(pool);
```

## Custom Allocators
As default, the standard lib's malloc, realloc and free are used to manage the memory behind the scenes. But custom functions can be overriden.
```C
void* custom_alloc(uint64_t size, void* user) {
    /*
        alloc imlementation
    */
}

void* custom_realloc(void* old_pointer, uint64_t old_size, uint64_t new_size, void* user) {
    /*
        realloc implementation
    */
}

void custom_free(void* pointer, uint64_t size, void* user) {
    /*
        free implementation
    */
}

rlpp_allocator_t custom_allocator = {
    .alloc = custom_alloc,
    .realloc = custom_realloc,
    .free = custom_free,
    .user = NULL,
};

int main() {

    my_type_t* pool = NULL;

    /*
        the second argument tells the init
        function whether the capacity is fixed
        or dynamic. if it's 0 it is dynamic and
        the realloc function may be called. if it
        is non-zero realloc will never be called,
        and the capacity will never grow

        caution: this macro will set pool to a
        non-null value on success. if the pointer
        is still null, it failed to initialize and
        should be handleded. otherwise, whenever a
        mutation happens, the default mode, with the
        default allocator and a dynamically growing
        capacity will be used, which might not be
        wanted.

        calling `rlpp_init_custom(pool, 0, NULL)`
        does the same as if it wasn't called at all
    */
    rlpp_init_custom(pool, 0, &custom_allocator);
    if(!pool) {
        //error
        return 1;
    }

    return 0;
}

```