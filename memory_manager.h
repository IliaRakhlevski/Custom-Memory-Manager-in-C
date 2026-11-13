#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>


/* union of the standard types with the strictest alignment requirements:
   every payload is aligned for it, so it can store any standard data type
   (C11 max_align_t is not available in the C library of MSVC) */
typedef union {
	long long ll;
	long double ld;
	void* p;
	void (*fp)(void);
} mm_max_align_t;

typedef enum {
	RET_CODE_SUCCESS,
	RET_CODE_INPUT_ERROR,
	RET_CODE_NOT_INIT,
	RET_CODE_MEM_SIZE_TOO_SMALL,
	RET_CODE_ALIGNMENT_ERROR,
	RET_CODE_ALLOC_FAILED,
	RET_CODE_INCORRECT_MEM_POINTER,
	RET_CODE_HEAP_CORRUPTED        /* an invalid block header was found while walking the heap */
} ret_code_t;


typedef struct{
	size_t block_size;  /* total block size: header + aligned payload */
	bool is_free;		/* indicates whether the block is free or allocated */
}header;

/* Initializes allocator with a user-provided memory block.
   On failure the previous state of the allocator is kept unchanged. */
ret_code_t my_malloc_init(void* mem_block, size_t mem_size);

/* Allocates at least `size` bytes, may return a larger aligned block */
ret_code_t my_malloc_alloc(size_t size, void** out_alloc_mem);

/* Frees memory previously returned by my_malloc_alloc */
ret_code_t my_malloc_free(void* mem_to_free);

/* Debug helper: prints current memory layout */
ret_code_t print_memory(void);

#endif /* MEM_MANAGER_H */
