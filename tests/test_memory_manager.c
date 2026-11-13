/* Unit tests for the custom memory manager.
   No test framework: every CHECK prints the failed condition, and the program
   returns a non-zero exit code if any check failed, so it can run in CI. */

#include <stddef.h>
#include <stdalign.h>
#include <string.h>
#include "memory_manager.h"

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                          \
	do {                                                                     \
		checks_run++;                                                        \
		if (!(cond)) {                                                       \
			checks_failed++;                                                 \
			printf("  FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__);     \
		}                                                                    \
	} while (0)

#define POOL_SIZE 4096

static alignas(mm_max_align_t) unsigned char pool[POOL_SIZE];

static bool is_aligned(const void* p)
{
	return (uintptr_t)p % alignof(mm_max_align_t) == 0;
}

static bool in_pool(const void* p, size_t size)
{
	const unsigned char* c = (const unsigned char*)p;
	return c >= pool && c + size <= pool + POOL_SIZE;
}

/* largest size that can be allocated from a fresh pool (found by binary search) */
static size_t max_alloc_size(void)
{
	size_t lo = 0, hi = POOL_SIZE;
	while (lo < hi)
	{
		size_t mid = lo + (hi - lo + 1) / 2;
		void* p = NULL;
		my_malloc_init(pool, POOL_SIZE);
		if (my_malloc_alloc(mid, &p) == RET_CODE_SUCCESS)
			lo = mid;
		else
			hi = mid - 1;
	}
	my_malloc_init(pool, POOL_SIZE);
	return lo;
}

/* runs first: the allocator has never been initialized */
static void test_not_initialized(void)
{
	void* p = NULL;
	printf("not initialized\n");
	CHECK(my_malloc_alloc(16, &p) == RET_CODE_NOT_INIT);
	CHECK(my_malloc_free(pool) == RET_CODE_NOT_INIT);
	CHECK(print_memory() == RET_CODE_SUCCESS);
}

static void test_init_errors(void)
{
	printf("init errors\n");
	CHECK(my_malloc_init(NULL, POOL_SIZE) == RET_CODE_INPUT_ERROR);
	CHECK(my_malloc_init(pool, 0) == RET_CODE_INPUT_ERROR);
	CHECK(my_malloc_init(pool, 8) == RET_CODE_MEM_SIZE_TOO_SMALL);
	CHECK(my_malloc_init((void*)(UINTPTR_MAX - 8), 64) == RET_CODE_INPUT_ERROR); /* end address would wrap */
}

static void test_failed_reinit_keeps_state(void)
{
	void* p = NULL;
	printf("failed re-init keeps the current heap\n");
	CHECK(my_malloc_init(pool, POOL_SIZE) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(32, &p) == RET_CODE_SUCCESS);
	CHECK(my_malloc_init(NULL, 100) == RET_CODE_INPUT_ERROR);
	CHECK(my_malloc_init(pool, 8) == RET_CODE_MEM_SIZE_TOO_SMALL);
	CHECK(my_malloc_free(p) == RET_CODE_SUCCESS);
}

static void test_alloc_input_errors(void)
{
	void* p = NULL;
	printf("alloc input errors\n");
	CHECK(my_malloc_init(pool, POOL_SIZE) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(0, &p) == RET_CODE_INPUT_ERROR);
	CHECK(my_malloc_alloc(16, NULL) == RET_CODE_INPUT_ERROR);
}

static void test_alignment(void)
{
	printf("every payload is aligned and inside the pool\n");
	for (size_t start = 0; start < 8; start++)          /* also an unaligned pool start */
	{
		CHECK(my_malloc_init(pool + start, POOL_SIZE - start) == RET_CODE_SUCCESS);
		for (size_t size = 1; size <= 40; size++)
		{
			void* p = NULL;
			CHECK(my_malloc_alloc(size, &p) == RET_CODE_SUCCESS);
			CHECK(is_aligned(p));
			CHECK(in_pool(p, size));
		}
	}
}

static void test_no_overlap(void)
{
	enum { N = 20 };
	unsigned char* p[N];
	printf("allocations do not overlap\n");
	CHECK(my_malloc_init(pool, POOL_SIZE) == RET_CODE_SUCCESS);
	for (int i = 0; i < N; i++)
	{
		size_t size = (size_t)(i * 7 + 1);
		CHECK(my_malloc_alloc(size, (void**)&p[i]) == RET_CODE_SUCCESS);
		memset(p[i], i + 1, size);
	}
	int overwritten = 0;
	for (int i = 0; i < N; i++)
		for (size_t k = 0; k < (size_t)(i * 7 + 1); k++)
			if (p[i][k] != (unsigned char)(i + 1))
				overwritten++;
	CHECK(overwritten == 0);
}

static void test_split_and_coalesce(void)
{
	void *a = NULL, *b = NULL, *c = NULL, *d = NULL;
	size_t max = max_alloc_size();
	printf("split and coalesce\n");

	CHECK(max > 0);
	CHECK(my_malloc_alloc(64, &a) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(64, &b) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(64, &c) == RET_CODE_SUCCESS);
	CHECK((unsigned char*)a < (unsigned char*)b && (unsigned char*)b < (unsigned char*)c);   /* first fit, split */

	/* free a and b: they merge, so a block bigger than 64 fits at the address of a */
	CHECK(my_malloc_free(b) == RET_CODE_SUCCESS);
	CHECK(my_malloc_free(a) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(128, &d) == RET_CODE_SUCCESS);
	CHECK(d == a);

	/* free everything: the whole pool is one free block again */
	CHECK(my_malloc_free(d) == RET_CODE_SUCCESS);
	CHECK(my_malloc_free(c) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(max, &d) == RET_CODE_SUCCESS);
	CHECK(my_malloc_free(d) == RET_CODE_SUCCESS);
}

static void test_exhaustion(void)
{
	void* p[POOL_SIZE];
	int n = 0;
	printf("exhaustion and reuse\n");
	CHECK(my_malloc_init(pool, POOL_SIZE) == RET_CODE_SUCCESS);
	while (n < POOL_SIZE && my_malloc_alloc(24, &p[n]) == RET_CODE_SUCCESS)
		n++;
	CHECK(n > 10);
	CHECK(my_malloc_alloc(24, &p[n]) == RET_CODE_ALLOC_FAILED);
	CHECK(my_malloc_alloc(POOL_SIZE, &p[n]) == RET_CODE_ALLOC_FAILED);
	for (int i = 0; i < n; i++)
		CHECK(my_malloc_free(p[i]) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(24, &p[0]) == RET_CODE_SUCCESS);
}

static void test_invalid_free(void)
{
	void* p = NULL;
	static unsigned char outside[64];
	printf("invalid free is rejected\n");
	CHECK(my_malloc_init(pool, POOL_SIZE) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(32, &p) == RET_CODE_SUCCESS);
	CHECK(my_malloc_free(NULL) == RET_CODE_INPUT_ERROR);
	CHECK(my_malloc_free(outside) == RET_CODE_INCORRECT_MEM_POINTER);
	CHECK(my_malloc_free((unsigned char*)p + 1) == RET_CODE_INCORRECT_MEM_POINTER);    /* interior pointer */
	CHECK(my_malloc_free(p) == RET_CODE_SUCCESS);
	CHECK(my_malloc_free(p) == RET_CODE_INCORRECT_MEM_POINTER);                        /* double free */
}

/* regression: header + size used to wrap around the address space, the call returned
   SUCCESS with a block smaller than its own header, and the next heap walk never ended */
static void test_huge_size_does_not_overflow(void)
{
	void* p = NULL;
	size_t max = max_alloc_size();
	size_t huge[] = { SIZE_MAX, SIZE_MAX - 7, SIZE_MAX - 15, SIZE_MAX - 64, SIZE_MAX / 2 + 1 };
	printf("huge sizes fail cleanly\n");

	CHECK(my_malloc_alloc(16, &p) == RET_CODE_SUCCESS);
	for (size_t i = 0; i < sizeof huge / sizeof huge[0]; i++)
	{
		void* q = NULL;
		CHECK(my_malloc_alloc(huge[i], &q) == RET_CODE_ALLOC_FAILED);
		CHECK(q == NULL);
	}
	/* the heap is intact: free and allocate the whole pool again */
	CHECK(print_memory() == RET_CODE_SUCCESS);
	CHECK(my_malloc_free(p) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(max, &p) == RET_CODE_SUCCESS);
}

static void test_corruption_detected(void)
{
	void *a = NULL, *b = NULL;
	header* first = (header*)pool;          /* the pool is aligned, so the first header is at its start */
	printf("corrupted header is detected, no endless loop\n");

	CHECK(my_malloc_init(pool, POOL_SIZE) == RET_CODE_SUCCESS);
	CHECK(my_malloc_alloc(32, &a) == RET_CODE_SUCCESS);

	first->block_size = 0;                  /* simulate a buffer overrun into the header */
	CHECK(my_malloc_alloc(32, &b) == RET_CODE_HEAP_CORRUPTED);
	CHECK(my_malloc_free(a) == RET_CODE_HEAP_CORRUPTED);
	CHECK(print_memory() == RET_CODE_HEAP_CORRUPTED);

	first->block_size = POOL_SIZE * 2;      /* a size beyond the managed memory */
	CHECK(my_malloc_alloc(32, &b) == RET_CODE_HEAP_CORRUPTED);
}

/* random allocations and frees, checked against a shadow list of live blocks */
static void test_random_stress(void)
{
	enum { SLOTS = 64, OPS = 20000 };
	unsigned char* ptr[SLOTS] = { 0 };
	size_t len[SLOTS] = { 0 };
	unsigned int seed = 12345;
	size_t max = max_alloc_size();
	int corrupted = 0, overlaps = 0, failed_free = 0;
	printf("random stress (%d operations)\n", OPS);

	for (int op = 0; op < OPS; op++)
	{
		seed = seed * 1103515245u + 12345u;
		int slot = (int)((seed >> 16) % SLOTS);

		if (ptr[slot] == NULL)
		{
			size_t size = 1 + (seed >> 8) % 96;
			void* p = NULL;
			if (my_malloc_alloc(size, &p) == RET_CODE_SUCCESS)
			{
				unsigned char* c = (unsigned char*)p;
				if (!is_aligned(c) || !in_pool(c, size))
					corrupted++;
				for (int s = 0; s < SLOTS; s++)
					if (ptr[s] != NULL && c < ptr[s] + len[s] && ptr[s] < c + size)
						overlaps++;
				memset(c, slot + 1, size);
				ptr[slot] = c;
				len[slot] = size;
			}
		}
		else
		{
			for (size_t k = 0; k < len[slot]; k++)
				if (ptr[slot][k] != (unsigned char)(slot + 1)) { corrupted++; break; }
			if (my_malloc_free(ptr[slot]) != RET_CODE_SUCCESS)
				failed_free++;
			ptr[slot] = NULL;
		}
	}
	CHECK(corrupted == 0);
	CHECK(overlaps == 0);
	CHECK(failed_free == 0);

	for (int s = 0; s < SLOTS; s++)
		if (ptr[s] != NULL)
			CHECK(my_malloc_free(ptr[s]) == RET_CODE_SUCCESS);

	/* after freeing everything the blocks are fully coalesced again */
	void* p = NULL;
	CHECK(my_malloc_alloc(max, &p) == RET_CODE_SUCCESS);
}

int main(void)
{
	test_not_initialized();
	test_init_errors();
	test_failed_reinit_keeps_state();
	test_alloc_input_errors();
	test_alignment();
	test_no_overlap();
	test_split_and_coalesce();
	test_exhaustion();
	test_invalid_free();
	test_huge_size_does_not_overflow();
	test_corruption_detected();
	test_random_stress();

	printf("\n%d checks, %d failed\n", checks_run, checks_failed);
	return checks_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
