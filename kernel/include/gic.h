#ifndef NOVA_GIC_H
#define NOVA_GIC_H
#include <stdint.h>
int nova_gic_init(uintptr_t dtb_address);
uint32_t nova_gic_acknowledge(void);
void nova_gic_end_interrupt(uint32_t interrupt_id);
#endif
