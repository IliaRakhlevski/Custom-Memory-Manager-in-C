#include <stddef.h>
#include <stdalign.h>
#include "memory_manager.h"


#define CHECK_RET if(ret) return ret;

/* payload alignment: suitable for any standard data type */
#define ALIGNMENT ((size_t)alignof(mm_max_align_t))

/* header size rounded up to the alignment, so the payload that follows a header is aligned too */
#define HEADER_SIZE ((sizeof(header) + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT)

/* the smallest valid block: a header and one aligned payload unit */
#define MIN_BLOCK_SIZE (HEADER_SIZE + ALIGNMENT)

static void* memory_block_aligned = NULL;

static uintptr_t memory_block_end_addr = 0;

static bool memory_block_init = false;


/* checks that the header of the block that starts at block_st is consistent:
   the block is not smaller than the minimal block and does not extend beyond the managed memory.
   Every function that walks the heap uses it, so a corrupted header stops the walk
   instead of causing an endless loop or an access outside the managed memory. */
static bool is_valid_block(uintptr_t block_st)
{
	const header* head = (const header*)block_st;

	if (memory_block_end_addr - block_st < MIN_BLOCK_SIZE)
		return false;

	return head->block_size >= MIN_BLOCK_SIZE && head->block_size <= memory_block_end_addr - block_st;
}

/* print the managed memory state
   debug helper: prints block list (header address, size, free/busy) */
ret_code_t print_memory(void)
{
	uintptr_t block_st;
	header* head = NULL;

	printf("\n=== MEMORY DUMP ===\n");

	if (!memory_block_init)
	{
		printf("Memory is not initialized\n\n");
		return RET_CODE_SUCCESS;
	}

	printf("Start of the aligned memory: %" PRIuPTR "\n", (uintptr_t)memory_block_aligned);
	printf("End of the aligned memory: %" PRIuPTR "\n", memory_block_end_addr);
	printf("Size of the aligned memory: %zu\n", (size_t)(memory_block_end_addr - (uintptr_t)memory_block_aligned));

	block_st = (uintptr_t)memory_block_aligned;

	while (block_st < memory_block_end_addr)
	{
		if (!is_valid_block(block_st))
		{
			printf("Corrupted block header at address %" PRIuPTR "\n\n", block_st);
			return RET_CODE_HEAP_CORRUPTED;
		}

		head = (header*)block_st;

		printf("Block: type - %s, size - %zu, address - %" PRIuPTR "\n", head->is_free ? "free" : "busy", head->block_size, block_st);

		block_st += head->block_size;
	}

	printf("\n\n");

	return RET_CODE_SUCCESS;
}

/* searching for the nearest align-up (ceil) address of the given pointer */
static ret_code_t find_nearest_aligned_address(uintptr_t addr, uintptr_t end_addr, uintptr_t* aligned_addres)
{
	uintptr_t al_mod = addr % ALIGNMENT;

	if (al_mod > 0)
	{
		/* the aligned address must not wrap around and must stay inside the given memory */
		if (ALIGNMENT - al_mod > end_addr - addr)
			return RET_CODE_ALIGNMENT_ERROR;
		addr += (ALIGNMENT - al_mod);
	}

	*aligned_addres = addr;

	return RET_CODE_SUCCESS;
}

/* initializing the memory manager, by receiving a continuous memory block and its size.
   The new state is computed in local variables and committed only on success,
   so a failed call does not break an allocator that is already initialized. */
ret_code_t my_malloc_init(void* mem_block, size_t mem_size)
{
	ret_code_t ret;
	uintptr_t aligned_memory_block = 0;
	uintptr_t start_addr, end_addr;


	if (mem_block == NULL || mem_size == 0)
		return RET_CODE_INPUT_ERROR;

	start_addr = (uintptr_t)mem_block;

	/* the end address must not wrap around */
	if (mem_size > UINTPTR_MAX - start_addr)
		return RET_CODE_INPUT_ERROR;

	end_addr = start_addr + mem_size;

	ret = find_nearest_aligned_address(start_addr, end_addr, &aligned_memory_block);
	CHECK_RET;

	if((end_addr - aligned_memory_block) < MIN_BLOCK_SIZE)
		return RET_CODE_MEM_SIZE_TOO_SMALL;

	header* head = (header*)aligned_memory_block;
	head->is_free = true;
	head->block_size = end_addr - aligned_memory_block;

	memory_block_aligned = (void*)aligned_memory_block;
	memory_block_end_addr = end_addr;
	memory_block_init = true;

	return RET_CODE_SUCCESS;
}

/* receiving a request for memory allocation for a given size
   allocator may return a block larger than requested due to alignment / non-splittable remainder */
ret_code_t my_malloc_alloc(size_t size, void** out_alloc_mem)
{
	uintptr_t block_st;
	header* head, *ex_head;

	if (out_alloc_mem == NULL || size == 0)
		return RET_CODE_INPUT_ERROR;

	if (!memory_block_init)
		return RET_CODE_NOT_INIT;

	block_st = (uintptr_t)memory_block_aligned;

	while (block_st < memory_block_end_addr)
	{
		if (!is_valid_block(block_st))
			return RET_CODE_HEAP_CORRUPTED;

		head = (header*)block_st;

		/* the size is compared with the free space of the block before any arithmetic on it:
		   adding a huge size to an address would wrap around (e.g. size == SIZE_MAX - 7) */
		if (head->is_free && size <= head->block_size - HEADER_SIZE)
		{
			/* payload rounded up to the alignment; cannot overflow, because
			   size is not larger than the block, and the block fits in the managed memory */
			size_t aligned_payload_size = (size + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
			size_t full_aligned_block_size = HEADER_SIZE + aligned_payload_size;

			if (full_aligned_block_size <= head->block_size)
			{
				size_t remains_free_block_size = head->block_size - full_aligned_block_size;

				/* split only if the remainder can form a valid block */
				if (remains_free_block_size >= MIN_BLOCK_SIZE)
				{
					ex_head = (header*)(block_st + full_aligned_block_size);
					ex_head->is_free = true;
					ex_head->block_size = remains_free_block_size;
					head->block_size = full_aligned_block_size;
				}

				head->is_free = false;
				*out_alloc_mem = (void*)(block_st + HEADER_SIZE);
				return RET_CODE_SUCCESS;
			}
		}

		block_st += head->block_size;
	}

	return RET_CODE_ALLOC_FAILED;
}

/* merging adjacent freed memory blocks
   keeps merging consecutive free blocks to the right (does not advance on merge) */
static ret_code_t merge_freed_blocks(void)
{
	uintptr_t block_st, next_st;
	header* head = NULL, *ex_head = NULL;

	block_st = (uintptr_t)memory_block_aligned;

	while (block_st < memory_block_end_addr)
	{
		if (!is_valid_block(block_st))
			return RET_CODE_HEAP_CORRUPTED;

		head = (header*)block_st;
		next_st = block_st + head->block_size;

		if (head->is_free && next_st < memory_block_end_addr)
		{
			if (!is_valid_block(next_st))
				return RET_CODE_HEAP_CORRUPTED;

			ex_head = (header*)next_st;
			if (ex_head->is_free)
			{
				head->block_size += ex_head->block_size;
				continue;
			}
		}
		block_st = next_st;
	}

	return RET_CODE_SUCCESS;
}

/* validates that ptr equals payload start of a currently allocated (busy) block */
static ret_code_t check_mem_ptr(void* mem_to_free, bool *valid)
{
	uintptr_t block_st;
	header* head = NULL;

	if (mem_to_free == NULL || valid == NULL)
			return RET_CODE_INPUT_ERROR;

	*valid = false;

	block_st = (uintptr_t)memory_block_aligned;

	while (block_st < memory_block_end_addr)
	{
		if (!is_valid_block(block_st))
			return RET_CODE_HEAP_CORRUPTED;

		head = (header*)block_st;
		if (!head->is_free)
		{
			if (block_st + HEADER_SIZE == (uintptr_t)mem_to_free)
			{
				*valid = true;
				break;
			}
		}
		block_st += head->block_size;
	}

	return RET_CODE_SUCCESS;
}

/* receiving a pointer to a previously allocated memory, and free it */
ret_code_t my_malloc_free(void* mem_to_free)
{
	uintptr_t mem_free_addr;
	header* head = NULL;
	bool valid;
	ret_code_t ret;

	if (mem_to_free == NULL)
		return RET_CODE_INPUT_ERROR;

	if(!memory_block_init)
		return RET_CODE_NOT_INIT;

	mem_free_addr = (uintptr_t)mem_to_free;

	/* range check */
	if (mem_free_addr < (uintptr_t)memory_block_aligned || mem_free_addr >= memory_block_end_addr)
	{
		return RET_CODE_INCORRECT_MEM_POINTER;
	}

	ret = check_mem_ptr(mem_to_free, &valid);
	CHECK_RET;

	if (!valid)
		return RET_CODE_INCORRECT_MEM_POINTER;

	head = (header*)(mem_free_addr - HEADER_SIZE);
	head->is_free = true;

	return merge_freed_blocks();
}
