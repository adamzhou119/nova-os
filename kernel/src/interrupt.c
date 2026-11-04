#include "interrupt.h"
#include "gic.h"
#include "timer.h"
#include "scheduler.h"

#define NOVA_TIMER_TICK_HZ 100U

static volatile uint64_t irq_ticks;

uintptr_t nova_irq_dispatch(uintptr_t frame_sp) {
    uint32_t interrupt_id = nova_gic_acknowledge();
    uintptr_t return_frame = frame_sp;
    if (interrupt_id == 30U) {
        ++irq_ticks;
        nova_sched_timer_tick();
        (void)nova_timer_arm_irq(nova_timer_frequency() / NOVA_TIMER_TICK_HZ);
    }
    if (interrupt_id < 1020U) nova_gic_end_interrupt(interrupt_id);

    /*
     * Only the timer can request preemption. EOI is issued before handing off
     * so the selected task never inherits an unacknowledged GIC interrupt.
     */
    if (interrupt_id == 30U) {
        uint32_t next_id = 0U;
        struct nova_irq_frame *next_frame = 0;
        if (nova_sched_irq_handoff((struct nova_irq_frame *)frame_sp,
                                   &next_id, &next_frame))
            return_frame = (uintptr_t)next_frame;
    }
    return return_frame;
}

uint64_t nova_irq_tick_count(void) { return irq_ticks; }
