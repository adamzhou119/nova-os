#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gic.h"
#include "interrupt.h"
#include "scheduler.h"
#include "timer.h"

static uint32_t next_interrupt;
static uint32_t ended_interrupt;
static unsigned int end_count;
static unsigned int timer_arm_count;
static unsigned int scheduler_tick_count;
static uint64_t armed_delta;
static unsigned int scheduler_handoff_count;
static int scheduler_handoff_result;
static struct nova_irq_frame *scheduler_handoff_frame;

int nova_gic_init(uintptr_t dtb_address) { (void)dtb_address; return 1; }
uint32_t nova_gic_acknowledge(void) { return next_interrupt; }
void nova_gic_end_interrupt(uint32_t id) {
    ended_interrupt = id;
    ++end_count;
}
uint64_t nova_timer_frequency(void) { return 1000000U; }
int nova_timer_arm_irq(uint64_t delta) {
    armed_delta = delta;
    ++timer_arm_count;
    return 1;
}
void nova_sched_timer_tick(void) { ++scheduler_tick_count; }
int nova_sched_irq_handoff(struct nova_irq_frame *frame, uint32_t *next_id,
                           struct nova_irq_frame **next_frame) {
    (void)frame;
    ++scheduler_handoff_count;
    if (!scheduler_handoff_result) return 0;
    *next_id = 99U;
    *next_frame = scheduler_handoff_frame;
    return 1;
}

int main(void) {
    struct nova_irq_frame frame = {0};
    uintptr_t frame_address = (uintptr_t)&frame;

    frame.x[0] = 0x0123456789abcdefULL;
    frame.x[16] = 0x1020304050607080ULL;
    frame.x[30] = 0xfedcba9876543210ULL;
    frame.elr_el1 = 0x40081234U;
    frame.spsr_el1 = 0x3c5U;

    next_interrupt = 30U;
    assert(nova_irq_dispatch(frame_address) == frame_address);
    assert(nova_irq_tick_count() == 1U);
    assert(scheduler_tick_count == 1U);
    assert(timer_arm_count == 1U);
    assert(armed_delta == 10000U);
    assert(end_count == 1U && ended_interrupt == 30U);
    assert(scheduler_handoff_count == 1U);

    next_interrupt = 33U;
    assert(nova_irq_dispatch(frame_address) == frame_address);
    assert(nova_irq_tick_count() == 1U);
    assert(scheduler_tick_count == 1U);
    assert(timer_arm_count == 1U);
    assert(end_count == 2U && ended_interrupt == 33U);
    assert(scheduler_handoff_count == 1U);

    next_interrupt = 1023U; /* GIC spurious interrupt: no EOI. */
    assert(nova_irq_dispatch(frame_address) == frame_address);
    assert(end_count == 2U);
    assert(scheduler_handoff_count == 1U);

    /* A timer handoff must return the selected task's frame to assembly. */
    {
        struct nova_irq_frame selected_frame = {0};
        scheduler_handoff_frame = &selected_frame;
        scheduler_handoff_result = 1;
        next_interrupt = 30U;
        assert(nova_irq_dispatch(frame_address) ==
               (uintptr_t)&selected_frame);
        assert(scheduler_handoff_count == 2U);
        assert(scheduler_tick_count == 2U);
        assert(timer_arm_count == 2U);
        assert(end_count == 3U && ended_interrupt == 30U);
    }

    assert(frame.x[0] == 0x0123456789abcdefULL);
    assert(frame.x[16] == 0x1020304050607080ULL);
    assert(frame.x[30] == 0xfedcba9876543210ULL);
    assert(frame.elr_el1 == 0x40081234U);
    assert(frame.spsr_el1 == 0x3c5U);

    puts("IRQ dispatcher host tests passed.");
    return 0;
}
