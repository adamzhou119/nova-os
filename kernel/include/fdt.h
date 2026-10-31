#ifndef NOVA_FDT_H
#define NOVA_FDT_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_FDT_MAX_MEMORY_REGIONS 8U
#define NOVA_FDT_MAX_RESERVED_REGIONS 16U

struct nova_memory_region {
    uint64_t start;
    uint64_t size;
};

/* Returns the number of memory regions, or a negative error code. */
int nova_fdt_memory_regions(uintptr_t dtb_address,
                            struct nova_memory_region *regions,
                            size_t capacity);

/* FDT header memreserve map plus /reserved-memory child reg entries. */
int nova_fdt_reserved_regions(uintptr_t dtb_address,
                              struct nova_memory_region *regions,
                              size_t capacity);

#endif
