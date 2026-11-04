#include "interrupt.h"
#include "gic.h"
#include "timer.h"
#include "scheduler.h"

#define NOVA_TIMER_TICK_HZ 100U

static volatile uint64_t irq_ticks;

/* Assembly-visible single-core IRQ return instrumentation. */
volatile uint64_t nova_irq_entry_tick;
volatile uint64_t nova_irq_handoff_last_ticks;
volatile uint64_t nova_irq_handoff_max_ticks;
volatile uint64_t nova_irq_handoff_count;
volatile uint32_t nova_irq_handoff_measure_pending;

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
                                   &next_id, &next_frame)) {
            nova_irq_handoff_measure_pending = 1U;
            return_frame = (uintptr_t)next_frame;
        }
    }
    return return_frame;
}

uint64_t nova_irq_tick_count(void) { return irq_ticks; }

void nova_irq_handoff_latency_reset(void) {
    nova_irq_handoff_count = 0U;
    nova_irq_handoff_last_ticks = 0U;
    nova_irq_handoff_max_ticks = 0U;
    nova_irq_handoff_measure_pending = 0U;
}

void nova_irq_handoff_latency_get(struct nova_irq_handoff_latency *metrics) {
    if (metrics == 0) return;
    metrics->count = nova_irq_handoff_count;
    metrics->last_ticks = nova_irq_handoff_last_ticks;
    metrics->max_ticks = nova_irq_handoff_max_ticks;
}
