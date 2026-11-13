#include "memory_manager.h"


#define REQ_MEM_SIZE 120

int main() 
{ 
    void* alloc_mem1 = NULL;
    void* alloc_mem2 = NULL;
    void* alloc_mem3 = NULL;

    /* allocate memory for the memory manager 
       malloc is used here only to obtain a test buffer;
       the memory manager itself performs no dynamic allocations */
    void* mem_block = malloc(REQ_MEM_SIZE);
    if (mem_block == NULL)
    {
        printf("Memory allocation error!");
        return EXIT_FAILURE;
    }

    /* sanity check: print_memory before init */
    print_memory();

    /* init allocator with fixed-size buffer */
    ret_code_t ret = my_malloc_init(mem_block, REQ_MEM_SIZE);
    if (ret > 0)
    {
        printf("my_malloc_init() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* alloc X bytes: expect split / alignment rounding */
    ret = my_malloc_alloc(20, &alloc_mem1);
    if (ret > 0)
    {
        printf("my_malloc_alloc() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* alloc X bytes: expect split / alignment rounding */
    ret = my_malloc_alloc(10, &alloc_mem2);
    if (ret > 0)
    {
        printf("my_malloc_alloc() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* alloc X bytes: expect split / alignment rounding */
    ret = my_malloc_alloc(20, &alloc_mem3);
    if (ret > 0)
    {
        printf("my_malloc_alloc() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* free block #N: expect merge only when adjacent blocks are free */
    ret = my_malloc_free(alloc_mem1);
    if (ret > 0)
    {
        printf("my_malloc_free() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* alloc X bytes: expect split / alignment rounding */
    ret = my_malloc_alloc(10, &alloc_mem1);
    if (ret > 0)
    {
        printf("my_malloc_alloc() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* free block #N: expect merge only when adjacent blocks are free */
    ret = my_malloc_free(alloc_mem3);
    if (ret > 0)
    {
        printf("my_malloc_free() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* free block #N: expect merge only when adjacent blocks are free */
    ret = my_malloc_free(alloc_mem1);
    if (ret > 0)
    {
        printf("my_malloc_free() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    /* free remaining blocks: expect full coalescing into a single free block */
    ret = my_malloc_free(alloc_mem2);
    if (ret > 0)
    {
        printf("my_malloc_free() error! - %d", ret);
        return EXIT_FAILURE;
    }

    print_memory();

    free(mem_block);
             
    return EXIT_SUCCESS;
}