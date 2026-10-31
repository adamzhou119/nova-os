#ifndef NOVA_FDT_H
#define NOVA_FDT_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_FDT_MAX_MEMORY_REGIONS 8U

struct nova_memory_region {
    uint64_t start;
    uint64_t size;
};

/* Returns the number of memory regions, or -1 for an invalid/unsupported DTB. */
int nova_fdt_memory_regions(uintptr_t dtb_address,
                            struct nova_memory_region *regions,
                            size_t capacity);

#endif
