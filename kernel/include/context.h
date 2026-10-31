#ifndef NOVA_CONTEXT_H
#define NOVA_CONTEXT_H

#include <stdint.h>

/* AAPCS64 context: x19..x30 at offsets 0..88; SP at offset 96. */
struct nova_cpu_context {
    uintptr_t x19_x30[12];
    uintptr_t sp;
};

int nova_arch_context_init(struct nova_cpu_context *context,
                           uintptr_t stack_top, uintptr_t entry,
                           uintptr_t argument0, uintptr_t argument1);
void nova_arch_context_switch(struct nova_cpu_context *from,
                              const struct nova_cpu_context *to);

#endif
