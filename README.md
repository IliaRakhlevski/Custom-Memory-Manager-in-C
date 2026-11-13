
# Custom Memory Manager in C

[![CI](https://github.com/IliaRakhlevski/Custom-Memory-Manager-in-C/actions/workflows/ci.yml/badge.svg)](https://github.com/IliaRakhlevski/Custom-Memory-Manager-in-C/actions/workflows/ci.yml)

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

Payloads are aligned to the strictest alignment of the standard types (`long long`, `long double`, pointers),
so a payload can store any standard data type: 16 bytes with GCC/Clang on x86-64, 8 bytes with MSVC.
C11 `max_align_t` is not used because the C library of MSVC does not provide it.

The header size is rounded up to this alignment, and the payload size is rounded up to it as well,
so every block - and therefore every payload - starts at an aligned address.

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

- Invalid input arguments, including a memory block whose end address would wrap around
- Use before initialization; a failed re-initialization keeps the current heap unchanged
- Requested sizes are compared with the free space of a block before any address arithmetic,
  so huge sizes (e.g. `SIZE_MAX - 7`) cannot wrap around and simply fail with `RET_CODE_ALLOC_FAILED`
- Every heap walk (allocation, free, coalescing, printing) validates each block header;
  a corrupted header returns `RET_CODE_HEAP_CORRUPTED` instead of an endless loop or an access outside the managed memory
- Invalid pointers passed to `free()`: outside the pool, interior pointers, double free

On detection of an invalid state, the operation fails gracefully without invoking undefined behavior.

---

## Public API

```c
ret_code_t my_malloc_init(void* mem_block, size_t mem_size);
ret_code_t my_malloc_alloc(size_t size, void** out_alloc_mem);
ret_code_t my_malloc_free(void* mem_to_free);
ret_code_t print_memory(void);
```

Return codes:

| Code | Meaning |
|---|---|
| `RET_CODE_SUCCESS` | The operation succeeded |
| `RET_CODE_INPUT_ERROR` | Invalid argument (`NULL`, zero size, memory block that wraps around) |
| `RET_CODE_NOT_INIT` | `my_malloc_init()` was not called successfully |
| `RET_CODE_MEM_SIZE_TOO_SMALL` | The memory block cannot hold even one minimal block |
| `RET_CODE_ALIGNMENT_ERROR` | The memory block is too small to be aligned |
| `RET_CODE_ALLOC_FAILED` | No free block is large enough |
| `RET_CODE_INCORRECT_MEM_POINTER` | The pointer was not returned by `my_malloc_alloc()` or is already freed |
| `RET_CODE_HEAP_CORRUPTED` | An invalid block header was found (e.g. after a buffer overrun) |

The allocator keeps its state in static variables: it manages one memory pool and is not thread-safe.

---

## Building and Testing

With Make (GCC, warnings as errors):

```bash
make            # demo program and unit tests
make test       # run the unit tests
make sanitize   # run the unit tests with AddressSanitizer and UndefinedBehaviorSanitizer
```

With CMake (GCC, Clang or MSVC):

```bash
cmake -B build -DMM_WERROR=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`tests/test_memory_manager.c` contains the unit tests: input validation, alignment of every payload,
splitting and coalescing, exhaustion and reuse, invalid and double free, huge sizes that used to overflow,
detection of a corrupted header, and a random stress test of 20,000 allocations and frees checked
against a shadow list of live blocks.

CI runs the tests on every push with GCC, Clang, GCC with sanitizers and MSVC.

---

## Project Files

- `memory_manager.h` / `memory_manager.c` - the memory manager
- `main.c` - demo program that prints the memory layout after each step
- `tests/test_memory_manager.c` - unit tests
- `Makefile`, `CMakeLists.txt` - build files
- `.github/workflows/ci.yml` - continuous integration
