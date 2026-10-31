#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "context.h"
#include "scheduler.h"
#include "task.h"

static _Alignas(16) unsigned char stack_pool[NOVA_TASK_MAX][4096];
static size_t next_stack;
static unsigned int switch_depth;
static uint32_t observed_task;

void nova_task_start_trampoline(void) {}

void *nova_vmap_alloc_page(unsigned int flags) {
    (void)flags;
    if (next_stack >= NOVA_TASK_MAX) return 0;
    return stack_pool[next_stack++];
}

int nova_vmap_free_page(void *address) {
    (void)address;
    return 1;
}

void nova_arch_context_switch(struct nova_cpu_context *from,
                             const struct nova_cpu_context *to) {
    (void)from;
    (void)to;
    if (switch_depth == 0U) {
        unsigned int i;
        assert(nova_sched_execution_phase() == NOVA_SCHED_TASK);
        observed_task = nova_sched_current();
        assert(observed_task != 0U);
        for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
        assert(nova_sched_preemption_pending());
        ++switch_depth;
        assert(nova_sched_block_current());
        --switch_depth;
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
        assert(nova_sched_current() == observed_task);
        nova_sched_timer_tick();
        assert(!nova_sched_preemption_pending());
    } else {
        assert(switch_depth == 1U);
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
    }
}

static void dummy_entry(void *argument) { (void)argument; }

int main(void) {
    uint32_t task_id;
    struct nova_task_info info;

    assert(nova_sched_execution_phase() == NOVA_SCHED_INACTIVE);
    assert(nova_sched_current() == 0U);
    nova_sched_timer_tick();
    assert(!nova_sched_preemption_pending());

    assert(nova_task_create(dummy_entry, 0, &task_id));
    assert(nova_sched_start());
    assert(nova_sched_execution_phase() == NOVA_SCHED_INACTIVE);
    assert(nova_sched_current() == 0U);
    assert(observed_task == task_id);
    assert(nova_task_get(task_id, &info));
    assert(info.state == NOVA_TASK_BLOCKED);
    assert(!nova_sched_preemption_pending());

    assert(nova_sched_unblock(task_id));
    assert(nova_task_transition(task_id, NOVA_TASK_RUNNING));
    assert(nova_task_transition(task_id, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(task_id));
    puts("Scheduler phase and cooperative handoff host tests passed.");
    return 0;
}
