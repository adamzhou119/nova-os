#ifndef NOVA_MMU_H
#define NOVA_MMU_H

#include <stdint.h>

/* Private EL0 virtual-address window shared by the page-table and task layers. */
#define NOVA_USER_VA_BASE 0x80000000UL
#define NOVA_USER_VA_END  0xC0000000UL

/* Enable the QEMU EL1 RAM/PL011 map and reserve a dynamic virtual window. */
int nova_mmu_enable(void);

/* Create/destroy a root with shared privileged mappings and a private user L2. */
int nova_mmu_address_space_create(uintptr_t *root_physical);
int nova_mmu_address_space_destroy(uintptr_t root_physical, uint16_t asid);
/* Switch TTBR0_EL1 using an 8-bit ASID; ASID zero is reserved for kernel root. */
int nova_mmu_switch_address_space(uintptr_t root_physical, uint16_t asid);
uintptr_t nova_mmu_kernel_root(void);
int nova_mmu_user_map_page(uintptr_t root_physical, uint16_t asid, uintptr_t virtual_address, uintptr_t physical_address, unsigned int flags);
int nova_mmu_user_unmap_page(uintptr_t root_physical, uint16_t asid, uintptr_t virtual_address);
int nova_mmu_user_protect_page(uintptr_t root_physical, uint16_t asid, uintptr_t virtual_address, unsigned int flags);
int nova_mmu_verify_user_access(uintptr_t virtual_address, int write);

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
int nova_mmu_translate(uintptr_t virtual_address,
                       uintptr_t *physical_address);

/* Verify that an address outside the prototype map faults translation. */
int nova_mmu_verify_unmapped(uintptr_t address);

/* Validate EL1 write access and inspect privileged execute permission bits. */
int nova_mmu_verify_writable(uintptr_t address);
int nova_mmu_verify_write_protected(uintptr_t address);
int nova_mmu_verify_executable(uintptr_t address);
int nova_mmu_verify_execute_never(uintptr_t address);

#endif
