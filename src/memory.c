
#include <reent.h>
#include <kernel.h>
#include <malloc.h>
#include <stdbool.h>
#include <memory.h>
#include <ee_tools.h>
#include <lz4_ps2.h>

typedef struct {
	size_t binary_size;
	size_t allocs_size;
	size_t stack_size;
} AthenaMemory;

static AthenaMemory prog_mem;

void *malloc(size_t size) {
    size_t* ptr = _malloc_r(_REENT, size);
    if (!ptr && size > 0) {
        if (athena_mem_compact_now(size) > 0)
            ptr = _malloc_r(_REENT, size);
    }

    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
}

void *realloc(void *memblock, size_t size) {
    size_t old_chunk_size = memblock ? ((size_t*)memblock)[-1] : 0;

    size_t* ptr = _realloc_r(_REENT, memblock, size);
    if (!ptr && size > 0) {
        if (athena_mem_compact_now(size) > 0)
            ptr = _realloc_r(_REENT, memblock, size);
    }

    if (ptr) {
        prog_mem.allocs_size -= old_chunk_size;
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    } else if (size == 0 && memblock) {
        prog_mem.allocs_size -= old_chunk_size;
    }

    return ptr;
}

void *calloc(size_t number, size_t size) {
    size_t* ptr = _calloc_r(_REENT, number, size);
    if (!ptr && number > 0 && size > 0) {
        if (athena_mem_compact_now(number * size) > 0)
            ptr = _calloc_r(_REENT, number, size);
    }

    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
}

void *memalign(size_t alignment, size_t size) {
    size_t* ptr = _memalign_r(_REENT, alignment, size);
    if (!ptr && size > 0) {
        if (athena_mem_compact_now(size) > 0)
            ptr = _memalign_r(_REENT, alignment, size);
    }

    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
}

void free(void* ptr) {
    if (ptr) {
        prog_mem.allocs_size -= ((size_t*)ptr)[-1];
    }

    _free_r(_REENT, ptr);
}


void init_memory_manager() {
    prog_mem.binary_size = (unsigned long)&_end - (unsigned long)&__start;
    prog_mem.stack_size = 0x20000;
    athena_mem_compress_init();
}

size_t get_binary_size() {  return prog_mem.binary_size;    }
size_t get_allocs_size() {  return prog_mem.allocs_size;    }
size_t get_stack_size() {  return prog_mem.stack_size;    }
size_t get_used_memory() {  return prog_mem.stack_size + prog_mem.allocs_size + prog_mem.binary_size;    }
