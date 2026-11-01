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
int nova_fdt_find_device_reg(uintptr_t dtb_address, const char *compatible,
                             size_t reg_index, struct nova_fdt_device *device);

/* Decode a three-cell ARM GICv2 interrupt specifier to a GIC INTID.
 * Cell 0 is SPI (0) or PPI (1); cell 1 is the interrupt number within
 * that class; cell 2 is the standard trigger/polarity flag (1, 2, 4, or 8).
 * Returns 1 on success, 0 for an unsupported/malformed specifier.
 */
int nova_fdt_decode_gicv2_interrupt(uint32_t type, uint32_t number,
                                    uint32_t flags, uint32_t *interrupt_id);

/* Find the first interrupt specifier for a compatible device node.
 * Resolves inherited/overridden interrupt-parent phandles and a three-cell
 * GICv2 interrupt-controller node. Returns 1 on success, 0 if the device has
 * no interrupts property, and a negative value for malformed or unsupported
 * interrupt bindings.
 */
int nova_fdt_find_gicv2_irq(uintptr_t dtb_address, const char *compatible,
                            uint32_t *interrupt_id);

/* Returns validated DTB totalsize, or zero for an invalid header. */
size_t nova_fdt_total_size(uintptr_t dtb_address);

/* FDT header memreserve map plus /reserved-memory child reg entries. */
int nova_fdt_reserved_regions(uintptr_t dtb_address,
                              struct nova_memory_region *regions,
                              size_t capacity);

#endif
