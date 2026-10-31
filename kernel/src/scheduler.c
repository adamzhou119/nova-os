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

static int switch_to_scheduler(enum nova_task_state next_state, int change_state) {
    struct nova_cpu_context *task_context;
    struct nova_task_info task_info;
    uintptr_t saved_daif = irq_save_disable();
    uint32_t id = current_task_id;

    if (execution_phase != NOVA_SCHED_TASK || id == 0U) {
        irq_restore(saved_daif);
        return 0;
    }
    task_context = nova_task_context(id);
    if (task_context == 0 ||
        !nova_task_get(id, &task_info)) {
        irq_restore(saved_daif);
        return 0;
    }
    /*
     * The first-run trampoline may already have marked the task ZOMBIE.
     * Do not attempt an illegal second RUNNING -> ZOMBIE transition.
     */
    if ((change_state || next_state == NOVA_TASK_ZOMBIE) &&
        !(next_state == NOVA_TASK_ZOMBIE &&
          task_info.state == NOVA_TASK_ZOMBIE) &&
        !nova_task_transition(id, next_state)) {
        irq_restore(saved_daif);
        return 0;
    }
    if (change_state &&
        !nova_task_set_resume_kind(id, NOVA_TASK_RESUME_COOPERATIVE)) {
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
    while (nova_task_next_ready(next_after_id, &next)) {
        struct nova_task_info next_info;
        uintptr_t saved_daif;

        if (!nova_task_get(next, &next_info) ||
            next_info.state != NOVA_TASK_READY)
            continue;

        saved_daif = irq_save_disable();
        if (next_info.resume_kind == NOVA_TASK_RESUME_IRQ) {
            struct nova_irq_frame *frame = 0;
            if (!nova_task_take_irq_context(next, &frame)) {
                irq_restore(saved_daif);
                continue;
            }

            current_task_id = next;
            next_after_id = next;
            current_quantum_ticks = 0U;
            preemption_pending = 0;
            execution_phase = NOVA_SCHED_TASK;

            /*
             * This restores the selected task's complete exception frame
             * and ERETs. It does not return on hardware. If that task later
             * yields, its saved cooperative context returns to the scheduler
             * continuation established by the first context switch below.
             */
            nova_arch_irq_resume(frame);
            /* Host-side scheduler tests provide a returning architecture shim. */
            irq_restore(saved_daif);
            continue;
        }

        {
            struct nova_cpu_context *context = nova_task_context(next);
            if (context == 0 ||
                !nova_task_transition(next, NOVA_TASK_RUNNING)) {
                irq_restore(saved_daif);
                continue;
            }
            if (!nova_task_set_resume_kind(next, NOVA_TASK_RESUME_ACTIVE)) {
                (void)nova_task_transition(next, NOVA_TASK_READY);
                irq_restore(saved_daif);
                continue;
            }
            current_task_id = next;
            next_after_id = next;
            current_quantum_ticks = 0U;
            preemption_pending = 0;
            execution_phase = NOVA_SCHED_TASK;
            nova_arch_context_switch(&scheduler_context, context);
        }

        execution_phase = NOVA_SCHED_SCHEDULER;
        {
            /*
             * An IRQ handoff may have resumed a different task before that
             * task yielded/exited to this shared scheduler continuation.
             * Reap the task that actually returned, not the stale 'next'
             * value from the original dispatch.
             */
            uint32_t returned_id = current_task_id;
            struct nova_task_info returned_task;
            if (returned_id != 0U &&
                nova_task_get(returned_id, &returned_task) &&
                returned_task.state == NOVA_TASK_ZOMBIE) {
                /* Reap only after execution has returned to the scheduler stack. */
                (void)nova_task_destroy(returned_id);
            }
        }
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

int nova_sched_irq_handoff(struct nova_irq_frame *frame, uint32_t *next_id,
                           struct nova_irq_frame **next_frame) {
    uintptr_t saved_daif = irq_save_disable();
    uint32_t current_id = current_task_id;
    int switched;

    if (execution_phase != NOVA_SCHED_TASK || current_id == 0U ||
        !preemption_pending || frame == 0 || next_id == 0 || next_frame == 0) {
        irq_restore(saved_daif);
        return 0;
    }

    switched = nova_task_irq_handoff(current_id, frame, next_id, next_frame);
    /* A no-peer result still consumes the expired quantum, per the ADR. */
    current_quantum_ticks = 0U;
    preemption_pending = 0;
    if (switched) {
        current_task_id = *next_id;
        next_after_id = *next_id;
    }
    irq_restore(saved_daif);
    return switched;
}

void nova_sched_clear_preemption(void) {
    current_quantum_ticks = 0U;
    preemption_pending = 0;
}

static void scheduler_wait_forever(void) {
#if defined(__aarch64__)
    for (;;) __asm__ volatile("wfe");
#else
    for (;;) __asm__ volatile("" ::: "memory");
#endif
}

void nova_sched_task_exit(void) {
    if (current_task_id == 0U) scheduler_wait_forever();
    (void)switch_to_scheduler(NOVA_TASK_ZOMBIE, 0);
    scheduler_wait_forever();
}
