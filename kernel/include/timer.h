#ifndef NOVA_TIMER_H
#define NOVA_TIMER_H

#include <stdint.h>

/* EL1 physical generic timer. Interrupt delivery remains masked in this stage. */
int nova_timer_init(void);
uint64_t nova_timer_frequency(void);
uint64_t nova_timer_now(void);
int nova_timer_arm(uint64_t delta_ticks);
int nova_timer_arm_irq(uint64_t delta_ticks);
int nova_timer_start_irq(uint64_t delta_ticks);
int nova_timer_expired(void);
void nova_timer_stop(void);

#endif
