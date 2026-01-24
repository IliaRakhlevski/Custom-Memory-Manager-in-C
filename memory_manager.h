#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>


typedef enum {
	RET_CODE_SUCCESS, RET_CODE_INPUT_ERROR, RET_CODE_NOT_INIT, RET_CODE_MEM_SIZE_TOO_SMALL, RET_CODE_ALIGNMENT_ERROR, RET_CODE_ALLOC_FAILED, RET_CODE_INCORRECT_MEM_POINTER
} ret_code_t;


typedef struct{
	size_t block_size;  /* total block size: header + aligned payload */
	bool is_free;		/* indicates whether the block is free or allocated */
}header;

/* Initializes allocator with a user-provided memory block */
ret_code_t my_malloc_init(void* mem_block, size_t mem_size);

/* Allocates at least `size` bytes, may return a larger aligned block */
ret_code_t my_malloc_alloc(size_t size, void** out_alloc_mem);

/* Frees memory previously returned by my_malloc_alloc */
ret_code_t my_malloc_free(void* mem_to_free);

/* Debug helper: prints current memory layout */
ret_code_t print_memory();

#endif MEM_MANAGER_H
