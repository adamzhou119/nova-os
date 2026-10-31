#ifndef NOVA_MMU_H
#define NOVA_MMU_H

#include <stdint.h>

/* Enable the QEMU EL1 RAM/PL011 map and reserve a dynamic virtual window. */
int nova_mmu_enable(void);

/* Verify that an EL1 stage-1 address translation preserves the address. */
int nova_mmu_verify_identity(uintptr_t address);

#define NOVA_MMU_MAP_WRITABLE  (1U << 0)
#define NOVA_MMU_MAP_EXECUTABLE (1U << 1)

/* Dynamically map/unmap one 4 KiB RAM page in the virtual-only window. */
int nova_mmu_map_page(uintptr_t virtual_address, uintptr_t physical_address,
                      unsigned int flags);
int nova_mmu_unmap_page(uintptr_t virtual_address);
int nova_mmu_protect_page(uintptr_t virtual_address, unsigned int flags);
int nova_mmu_verify_translation(uintptr_t virtual_address,
                                uintptr_t physical_address);

/* Verify that an address outside the prototype map faults translation. */
int nova_mmu_verify_unmapped(uintptr_t address);

/* Validate EL1 write access and inspect privileged execute permission bits. */
int nova_mmu_verify_writable(uintptr_t address);
int nova_mmu_verify_write_protected(uintptr_t address);
int nova_mmu_verify_executable(uintptr_t address);
int nova_mmu_verify_execute_never(uintptr_t address);

#endif
