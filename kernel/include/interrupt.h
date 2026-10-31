#ifndef NOVA_INTERRUPT_H
#define NOVA_INTERRUPT_H
#include <stdint.h>
void nova_irq_dispatch(void);
uint64_t nova_irq_tick_count(void);
#endif
