#ifndef NOVA_MMU_H
#define NOVA_MMU_H

#include <stdint.h>

/* Enable a minimal EL1 identity map for QEMU virt RAM and PL011 UART. */
int nova_mmu_enable(void);

/* Verify that an EL1 stage-1 address translation preserves the address. */
int nova_mmu_verify_identity(uintptr_t address);

#endif
