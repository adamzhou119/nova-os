#ifndef NOVA_VM_H
#define NOVA_VM_H

#include "mmu.h"

/* Single-page kernel virtual allocator for the reserved QEMU virtual window. */
void *nova_vmap_alloc_page(unsigned int flags);
int nova_vmap_free_page(void *virtual_address);
/* Stack allocation reserves an unmapped guard page immediately below the stack. */
void *nova_vmap_alloc_guarded_page(unsigned int flags);
int nova_vmap_free_guarded_page(void *stack_address);

#endif
