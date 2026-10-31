#include "interrupt.h"
#include "gic.h"
#include "timer.h"
#include "scheduler.h"

#define NOVA_TIMER_TICK_HZ 100U

static volatile uint64_t irq_ticks;

uintptr_t nova_irq_dispatch(uintptr_t frame_sp) {
    uint32_t interrupt_id = nova_gic_acknowledge();
    if (interrupt_id == 30U) {
        ++irq_ticks;
        nova_sched_timer_tick();
        (void)nova_timer_arm_irq(nova_timer_frequency() / NOVA_TIMER_TICK_HZ);
    }
    if (interrupt_id < 1020U) nova_gic_end_interrupt(interrupt_id);

    /*
     * The return-frame ABI is ready for IRQ scheduling. Until a scheduler
     * implementation selects another task frame, resume the interrupted one.
     */
    return frame_sp;
}

uint64_t nova_irq_tick_count(void) { return irq_ticks; }
