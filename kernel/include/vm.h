#ifndef NOVA_VM_H
#define NOVA_VM_H

#include "mmu.h"

/* Single-page kernel virtual allocator for the reserved QEMU virtual window. */
void *nova_vmap_alloc_page(unsigned int flags);
int nova_vmap_free_page(void *virtual_address);

#endif
