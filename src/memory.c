
#include <reent.h>
#include <kernel.h>
#include <malloc.h>
#include <stdbool.h>
#include "include/memory.h"
#include "include/ee_tools.h"

extern char __start;
extern char _end;

typedef struct {
	size_t binary_size;
	size_t allocs_size;
	size_t stack_size;
} AthenaMemory;

static AthenaMemory prog_mem;

static bool sema_started = false;

static s32 malloc_semaid;
static ee_sema_t malloc_sema;

static int malloc_locked = 0;
static int malloc_lock_count = 0;

extern void __malloc_lock(struct _reent *r) {
    if(!sema_started) {
        malloc_sema.init_count = 1;
        malloc_sema.max_count = 1;
        malloc_sema.option = 0;
        malloc_semaid = CreateSema(&malloc_sema);
        sema_started = true;
    }
    if (!malloc_locked) {
        WaitSema(malloc_semaid);
        malloc_locked = 1;
    }
    malloc_lock_count++;
}

extern void __malloc_unlock(struct _reent *r) {
    malloc_lock_count--;
    if (malloc_lock_count == 0) {
        malloc_locked = 0;
        SignalSema(malloc_semaid);
    }
}


void *tracked_malloc(size_t size) {
    size_t* ptr = _malloc_r(_REENT, size);

    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
    
}

void *tracked_realloc(void *memblock, size_t size) {
    prog_mem.allocs_size -= ((size_t*)memblock)[-1];
    size_t* ptr = _realloc_r(_REENT, memblock, size);
    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
}

void *tracked_calloc(size_t number, size_t size) {
    size_t* ptr = _calloc_r(_REENT, number, size);
    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
}

void *tracked_memalign(size_t alignment, size_t size) {
    size_t* ptr = _memalign_r(_REENT, alignment, size);
    if (ptr) {
        prog_mem.allocs_size += ((size_t*)ptr)[-1];
    }

    return ptr;
}

void tracked_free(void* ptr) {
    prog_mem.allocs_size -= ((size_t*)ptr)[-1];
    _free_r(_REENT, ptr);
}

void init_memory_manager() {
    prog_mem.binary_size = (unsigned long)&_end - (unsigned long)&__start;
    prog_mem.stack_size = 0x20000;

    RedirectFunction(memalign, tracked_memalign);
    RedirectFunction(malloc, tracked_malloc);
    RedirectFunction(realloc, tracked_realloc);
    RedirectFunction(calloc, tracked_calloc);
    RedirectFunction(free, tracked_free);
}

size_t get_binary_size() {  return prog_mem.binary_size;    }
size_t get_allocs_size() {  return prog_mem.allocs_size;    }
size_t get_stack_size() {  return prog_mem.stack_size;    }
size_t get_used_memory() {  return prog_mem.stack_size + prog_mem.allocs_size + prog_mem.binary_size;    }