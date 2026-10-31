#ifndef NOVA_MEMORY_H
#define NOVA_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "fdt.h"

/* Bootstrap monotonic heap: single-core only, no free(). */
int nova_heap_init(uintptr_t start, uintptr_t end);
void *nova_alloc(size_t size, size_t alignment);
size_t nova_heap_used(void);
size_t nova_heap_available(void);

/* 4 KiB physical page allocator: single-core, no MMU integration yet. */
#define NOVA_PAGE_SIZE 4096U
int nova_page_init(uintptr_t start, uintptr_t end);
int nova_page_init_regions(const struct nova_memory_region *memory,
                           size_t memory_count,
                           const struct nova_memory_region *reserved,
                           size_t reserved_count);
void *nova_page_alloc(void);
int nova_page_free(void *page);
size_t nova_page_total(void);
size_t nova_page_available(void);

#endif
