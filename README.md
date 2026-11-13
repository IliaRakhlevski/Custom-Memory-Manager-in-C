
# Custom Memory Manager in C

This project implements a simple custom memory manager in C, operating on a fixed-size user-provided memory buffer.

The goal of the project is to demonstrate low-level memory management concepts such as alignment, block splitting, block coalescing, and defensive handling of invalid inputs.

The implementation is intentionally minimal and focused on correctness and clarity rather than performance or feature completeness.

---

## Overview

The memory manager operates on a single contiguous memory pool provided by the user at initialization time.  
No dynamic memory allocation (`malloc`, `free`, etc.) is used internally by the memory manager itself.

Each memory block contains a small header followed by an aligned payload area.

---

## Features

- Fixed-size memory pool
- Header-based block layout
- Payload alignment
- First-fit allocation strategy
- Block splitting on allocation
- Block coalescing on free
- Pointer validation in `free()`
- Defensive checks against heap corruption
- Debug function to print current memory layout

---

## Block Layout

Each memory block consists of:

- **Header**
  - Total block size (header + aligned payload)
  - Allocation status (free / allocated)
- **Payload**
  - Starts immediately after the header
  - Payload size is rounded up to satisfy alignment requirements

No explicit padding field is stored; any extra bytes introduced by alignment are accounted for implicitly via the block size.

---

## Alignment

Payloads are aligned to `sizeof(void*)`, which is sufficient for typical platforms and allows storing any standard data type.

Alignment is achieved by rounding up the end of the payload to the required boundary.

---

## Allocation Policy

- First-fit strategy is used to locate a suitable free block.
- If a free block is larger than required:
  - It is split **only if** the remaining space can form a valid block.
  - Otherwise, the entire block is allocated to avoid creating unusable fragments.
- The allocator may return more memory than requested due to alignment or split constraints, but never less.

---

## Free Policy

- Only pointers returned by `my_malloc_alloc()` are accepted.
- `free()` validates that the pointer matches the payload start of an allocated block.
- Interior pointers and invalid addresses are rejected.
- Adjacent free blocks are merged (coalesced) to reduce fragmentation.

---

## Error Handling

The implementation performs defensive validation at multiple stages:

- Invalid input arguments
- Use before initialization
- Invalid or corrupted block headers
- Invalid pointers passed to `free()`

On detection of an invalid state, the operation fails gracefully without invoking undefined behavior.

---

## Public API

```c
ret_code_t my_malloc_init(void* mem_block, size_t mem_size);
ret_code_t my_malloc_alloc(size_t size, void** out_alloc_mem);
ret_code_t my_malloc_free(void* mem_to_free);
ret_code_t print_memory(void);
