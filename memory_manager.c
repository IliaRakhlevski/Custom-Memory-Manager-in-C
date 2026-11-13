#include "memory_manager.h"


#define CHECK_RET if(ret) return ret;  

static size_t alignment_size = sizeof(void*);

static void* memory_block_aligned = NULL;

static uintptr_t memory_block_end_addr = 0;

static bool memory_block_init = false;

static size_t min_block_size = 0;


/* print the managed memory state 
   debug helper: prints block list (header address, size, free/busy) 
   assumes heap is not corrupted */
ret_code_t print_memory()
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

	while (block_st + sizeof(header) <= memory_block_end_addr)
	{
		head = (header*)block_st;

		printf("Block: type - %s, size - %zu, address - %" PRIuPTR "\n", head->is_free ? "free" : "busy", head->block_size, block_st);

		block_st += head->block_size;
	}

	printf("\n\n");

	return RET_CODE_SUCCESS;
}

/* searching for the nearest align-up (ceil) address of the given pointer */
static ret_code_t find_nearest_aligned_address(void* mem_block, uintptr_t* aligned_addres)
{
	uintptr_t u_mem_block = (uintptr_t)mem_block;

	uintptr_t al_mod = u_mem_block% alignment_size;

	if (al_mod > 0)
		u_mem_block += (alignment_size - al_mod);

	/* returns ALIGNMENT_ERROR if aligned address goes beyond managed memory end */
	if (u_mem_block > memory_block_end_addr)
		return RET_CODE_ALIGNMENT_ERROR;

	*aligned_addres = u_mem_block;

	return RET_CODE_SUCCESS;
}

/* initializing the memory manager, by receiving a continuous memory block and its size */
ret_code_t my_malloc_init(void* mem_block, size_t mem_size)
{
	ret_code_t ret;
	uintptr_t aligned_memory_block = 0;


	if (mem_block == NULL || mem_size == 0)
		return RET_CODE_INPUT_ERROR;


	min_block_size = sizeof(header) + alignment_size;

	memory_block_end_addr = (uintptr_t)mem_block + mem_size;

	ret = find_nearest_aligned_address(mem_block, &aligned_memory_block);
	CHECK_RET;

	memory_block_aligned = (void*)aligned_memory_block;

	if((memory_block_end_addr - aligned_memory_block) < min_block_size)
		return RET_CODE_MEM_SIZE_TOO_SMALL;

	header* head = (header*)memory_block_aligned;
	head->is_free = true;
	head->block_size = memory_block_end_addr - aligned_memory_block;

	memory_block_init = true;

	return RET_CODE_SUCCESS;
}

/* receiving a request for memory allocation for a given size 
   allocator may return a block larger than requested due to alignment / non-splittable remainder */
ret_code_t my_malloc_alloc(size_t size, void** out_alloc_mem)
{
	uintptr_t block_st;
	header* head, *ex_head;
	ret_code_t ret, ret_code = RET_CODE_ALLOC_FAILED;

	if (out_alloc_mem == NULL || size == 0)
		return RET_CODE_INPUT_ERROR;

	if (!memory_block_init)
		return RET_CODE_NOT_INIT;

	block_st = (uintptr_t)memory_block_aligned;

	while (block_st + sizeof(header) <= memory_block_end_addr)
	{
		head = (header*)block_st;
		if (head->block_size < min_block_size)
			return RET_CODE_ALLOC_FAILED;

		if (!head->is_free) 
		{
			block_st += head->block_size;
		}
		else
		{
			uintptr_t end_of_block_aligned;
			uintptr_t end_of_block = block_st + sizeof(header) + size;

			ret = find_nearest_aligned_address((void*)end_of_block, &end_of_block_aligned);
			CHECK_RET;

			size_t full_aligned_block_size = end_of_block_aligned - block_st;

			if (full_aligned_block_size > head->block_size)
			{
				block_st += head->block_size;
				continue;
			}

			size_t remains_free_block_size = head->block_size - full_aligned_block_size;

			if (remains_free_block_size >= min_block_size)
			{
				ex_head = (header*)end_of_block_aligned;
				ex_head->is_free = true;
				ex_head->block_size = remains_free_block_size;
				head->block_size = (size_t)ex_head - (size_t)head;
			}		

			head->is_free = false;
			*out_alloc_mem = (void*)(block_st + sizeof(header));
			ret_code = RET_CODE_SUCCESS;
			break;
		}
	}

	return ret_code;
}

/* merging adjacent freed memory blocks 
   keeps merging consecutive free blocks to the right (does not advance on merge) */
static ret_code_t merge_freed_blocks()
{
	uintptr_t block_st;
	header* head = NULL, *ex_head = NULL;

	block_st = (uintptr_t)memory_block_aligned;

	while (block_st + sizeof(header) <= memory_block_end_addr)
	{
		head = (header*)block_st;
		if (head->is_free)
		{
			ex_head = (header * )((uintptr_t)head + head->block_size);
			if (((uintptr_t)ex_head + sizeof(header) <= memory_block_end_addr) && (ex_head->is_free))
			{
				head->block_size += ex_head->block_size;
				continue;
			}
		}
		block_st += head->block_size;
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

	while (block_st + sizeof(header) <= memory_block_end_addr)
	{
		head = (header*)block_st;
		if (!head->is_free)
		{
			if ((uintptr_t)head + sizeof(header) == (uintptr_t)mem_to_free)
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

	head = (header*)((uintptr_t)mem_to_free - sizeof(header));
	head->is_free = true;

	merge_freed_blocks();

	return RET_CODE_SUCCESS;
}
