#include "scheduler.h"

#include "context.h"
#include "task.h"

#define NOVA_SCHED_QUANTUM_TICKS 5U

static struct nova_cpu_context scheduler_context;
static volatile uint32_t current_task_id;
static uint32_t next_after_id;
static volatile enum nova_sched_phase execution_phase = NOVA_SCHED_INACTIVE;
static volatile uint32_t current_quantum_ticks;
static volatile int preemption_pending;

static uintptr_t irq_save_disable(void) {
#if defined(__aarch64__)
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2"
                     : "=r"(daif) :: "memory");
    return daif;
#else
    return 0U;
#endif
}

static void irq_restore(uintptr_t daif) {
#if defined(__aarch64__)
    __asm__ volatile("msr daif, %0" :: "r"(daif) : "memory");
#else
    (void)daif;
#endif
}

static int find_next_ready(uint32_t *task_id) {
    return nova_task_next_ready(next_after_id, task_id);
}

static int switch_to_scheduler(enum nova_task_state next_state, int change_state) {
    struct nova_cpu_context *task_context;
    uintptr_t saved_daif = irq_save_disable();
    uint32_t id = current_task_id;

    if (execution_phase != NOVA_SCHED_TASK || id == 0U) {
        irq_restore(saved_daif);
        return 0;
    }
    task_context = nova_task_context(id);
    if (task_context == 0 ||
        (change_state && !nova_task_transition(id, next_state))) {
        irq_restore(saved_daif);
        return 0;
    }

    current_quantum_ticks = 0U;
    preemption_pending = 0;
    execution_phase = NOVA_SCHED_SCHEDULER;
    nova_arch_context_switch(task_context, &scheduler_context);

    /* Reached only when this same task is selected and resumed later. */
    irq_restore(saved_daif);
    return 1;
}

int nova_sched_start(void) {
    uint32_t next;
    if (execution_phase != NOVA_SCHED_INACTIVE) return 0;

    execution_phase = NOVA_SCHED_SCHEDULER;
    current_task_id = 0U;
    while (find_next_ready(&next)) {
        struct nova_cpu_context *context = nova_task_context(next);
        uintptr_t saved_daif;
        if (context == 0) continue;

        saved_daif = irq_save_disable();
        if (!nova_task_transition(next, NOVA_TASK_RUNNING)) {
            irq_restore(saved_daif);
            continue;
        }
        current_task_id = next;
        next_after_id = next;
        current_quantum_ticks = 0U;
        preemption_pending = 0;
        execution_phase = NOVA_SCHED_TASK;
        nova_arch_context_switch(&scheduler_context, context);

        execution_phase = NOVA_SCHED_SCHEDULER;
        current_task_id = 0U;
        irq_restore(saved_daif);
    }
    execution_phase = NOVA_SCHED_INACTIVE;
    return 1;
}

int nova_sched_yield(void) {
    return switch_to_scheduler(NOVA_TASK_READY, 1);
}

int nova_sched_block_current(void) {
    return switch_to_scheduler(NOVA_TASK_BLOCKED, 1);
}

int nova_sched_unblock(uint32_t task_id) {
    uintptr_t saved_daif = irq_save_disable();
    int result = nova_task_transition(task_id, NOVA_TASK_READY);
    irq_restore(saved_daif);
    return result;
}

uint32_t nova_sched_current(void) {
    return current_task_id;
}

enum nova_sched_phase nova_sched_execution_phase(void) {
    return execution_phase;
}

void nova_sched_timer_tick(void) {
    if (execution_phase != NOVA_SCHED_TASK || current_task_id == 0U ||
        preemption_pending) return;
    if (++current_quantum_ticks >= NOVA_SCHED_QUANTUM_TICKS)
        preemption_pending = 1;
}

int nova_sched_preemption_pending(void) { return preemption_pending; }

void nova_sched_clear_preemption(void) {
    current_quantum_ticks = 0U;
    preemption_pending = 0;
}

void nova_sched_task_exit(void) {
    if (current_task_id == 0U) for (;;) __asm__ volatile("wfe");
    (void)switch_to_scheduler(NOVA_TASK_ZOMBIE, 0);
    for (;;) __asm__ volatile("wfe");
}
