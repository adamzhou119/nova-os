#ifndef NOVA_FDT_H
#define NOVA_FDT_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_FDT_MAX_MEMORY_REGIONS 8U
#define NOVA_FDT_MAX_RESERVED_REGIONS 16U
#define NOVA_FDT_COMPATIBLE_MAX 64U

struct nova_fdt_device {
    uintptr_t mmio_base;
    size_t mmio_size;
};

struct nova_memory_region {
    uint64_t start;
    uint64_t size;
};

/* Returns the number of memory regions, or a negative error code. */
int nova_fdt_memory_regions(uintptr_t dtb_address,
                            struct nova_memory_region *regions,
                            size_t capacity);

/* Find the first node with an exact compatible string and a valid first reg tuple.
 * Returns 1 on success, 0 when absent, or a negative value for malformed input.
 * The reg tuple is decoded using parent bus cell widths and translated through
 * each ancestor bus's ranges property. Missing nested-bus ranges are rejected.
 */
int nova_fdt_find_device(uintptr_t dtb_address, const char *compatible,
                         struct nova_fdt_device *device);

/* Returns validated DTB totalsize, or zero for an invalid header. */
size_t nova_fdt_total_size(uintptr_t dtb_address);

/* FDT header memreserve map plus /reserved-memory child reg entries. */
int nova_fdt_reserved_regions(uintptr_t dtb_address,
                              struct nova_memory_region *regions,
                              size_t capacity);

#endif
