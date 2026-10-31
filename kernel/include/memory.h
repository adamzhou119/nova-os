#ifndef NOVA_MEMORY_H
#define NOVA_MEMORY_H

#include <stddef.h>
#include <stdint.h>

/* Bootstrap monotonic allocator. No free(), paging, or SMP locking yet. */
int nova_heap_init(uintptr_t start, uintptr_t end);
void *nova_alloc(size_t size, size_t alignment);
size_t nova_heap_used(void);
size_t nova_heap_available(void);

#endif
