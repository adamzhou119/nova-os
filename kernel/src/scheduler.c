#include "scheduler.h"

#include "context.h"
#include "task.h"

#include <stddef.h>

#define NOVA_SCHED_MAX_TASKS NOVA_TASK_MAX
#define NOVA_SCHED_QUANTUM_TICKS 5U

static struct nova_cpu_context scheduler_context;
static uint32_t current_task_id;
static uint32_t next_after_id;
static int scheduler_active;
static volatile uint32_t current_quantum_ticks;
static volatile int preemption_pending;

static int find_next_ready(uint32_t *task_id) {
    return nova_task_next_ready(next_after_id, task_id);
}

static int switch_to_scheduler(void) {
    struct nova_cpu_context *task_context;
    if (!scheduler_active || current_task_id == 0U) return 0;
    task_context = nova_task_context(current_task_id);
    if (task_context == 0) return 0;
    nova_arch_context_switch(task_context, &scheduler_context);
    return 1;
}

int nova_sched_start(void) {
    uint32_t next;
    if (scheduler_active) return 0;
    scheduler_active = 1;
    current_task_id = 0U;
    while (find_next_ready(&next)) {
        struct nova_cpu_context *context = nova_task_context(next);
        if (context == 0 || !nova_task_transition(next, NOVA_TASK_RUNNING))
            continue;
        current_task_id = next;
        next_after_id = next;
        nova_arch_context_switch(&scheduler_context, context);
        current_task_id = 0U;
    }
    scheduler_active = 0;
    return 1;
}

int nova_sched_yield(void) {
    uint32_t id = current_task_id;
    current_quantum_ticks = 0U;
    preemption_pending = 0;
    if (id == 0U || !nova_task_transition(id, NOVA_TASK_READY)) return 0;
    return switch_to_scheduler();
}

int nova_sched_block_current(void) {
    uint32_t id = current_task_id;
    current_quantum_ticks = 0U;
    preemption_pending = 0;
    if (id == 0U || !nova_task_transition(id, NOVA_TASK_BLOCKED)) return 0;
    return switch_to_scheduler();
}

int nova_sched_unblock(uint32_t task_id) {
    return nova_task_transition(task_id, NOVA_TASK_READY);
}

uint32_t nova_sched_current(void) {
    return current_task_id;
}

void nova_sched_timer_tick(void) {
    if (!scheduler_active || current_task_id == 0U || preemption_pending) return;
    if (++current_quantum_ticks >= NOVA_SCHED_QUANTUM_TICKS) preemption_pending = 1;
}
int nova_sched_preemption_pending(void) { return preemption_pending; }
void nova_sched_clear_preemption(void) { current_quantum_ticks = 0U; preemption_pending = 0; }

void nova_sched_task_exit(void) {
    if (current_task_id == 0U) for (;;) __asm__ volatile("wfe");
    (void)switch_to_scheduler();
    for (;;) __asm__ volatile("wfe");
}
