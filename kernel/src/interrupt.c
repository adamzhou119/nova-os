#include "interrupt.h"
#include "gic.h"
#include "timer.h"
#define NOVA_TIMER_TICK_HZ 100U
static volatile uint64_t irq_ticks;
void nova_irq_dispatch(void) {
    uint32_t interrupt_id = nova_gic_acknowledge();
    if (interrupt_id == 30U) {
        ++irq_ticks;
        (void)nova_timer_arm_irq(nova_timer_frequency() / NOVA_TIMER_TICK_HZ);
    }
    if (interrupt_id < 1020U) nova_gic_end_interrupt(interrupt_id);
}
uint64_t nova_irq_tick_count(void) { return irq_ticks; }
