#include "memory.h"

static uintptr_t heap_begin;
static uintptr_t heap_cursor;
static uintptr_t heap_limit;
static int heap_ready;

static int is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1U)) == 0;
}

int nova_heap_init(uintptr_t start, uintptr_t end) {
    if (start == 0 || end <= start) {
        heap_ready = 0;
        return 0;
    }

    heap_begin = start;
    heap_cursor = start;
    heap_limit = end;
    heap_ready = 1;
    return 1;
}

void *nova_alloc(size_t size, size_t alignment) {
    uintptr_t aligned;
    uintptr_t mask;

    if (!heap_ready || size == 0 || !is_power_of_two(alignment)) {
        return NULL;
    }

    mask = (uintptr_t)alignment - 1U;
    if (heap_cursor > UINTPTR_MAX - mask) {
        return NULL;
    }

    aligned = (heap_cursor + mask) & ~mask;
    if (aligned > heap_limit || size > (size_t)(heap_limit - aligned)) {
        return NULL;
    }

    heap_cursor = aligned + size;
    return (void *)aligned;
}

size_t nova_heap_used(void) {
    return heap_ready ? (size_t)(heap_cursor - heap_begin) : 0;
}

size_t nova_heap_available(void) {
    return heap_ready ? (size_t)(heap_limit - heap_cursor) : 0;
}
