#include "scheduler.h"

#include "context.h"
#include "task.h"
#include "address_space.h"
#include "mmu.h"

#define NOVA_SCHED_QUANTUM_TICKS 5U

static struct nova_cpu_context scheduler_context;
static volatile uint32_t current_task_id;
static uint32_t next_after_id;
static volatile enum nova_sched_phase execution_phase = NOVA_SCHED_INACTIVE;
static volatile uint32_t current_quantum_ticks;
static volatile int preemption_pending;

static uint32_t active_address_space_id;
static void scheduler_wait_forever(void);
__attribute__((weak)) uintptr_t nova_mmu_kernel_root(void) { return 0U; }
__attribute__((weak)) int nova_mmu_switch_address_space(uintptr_t root, uint16_t asid) {
    (void)root; (void)asid; return 1;
}
/*
 * Indirection keeps address-space reference failures injectable in host tests.
 * Production builds use the registry operations directly.
 */
__attribute__((weak)) int nova_sched_address_space_activate(uint32_t id) {
    return nova_address_space_activate(id);
}
__attribute__((weak)) int nova_sched_address_space_deactivate(uint32_t id) {
    return nova_address_space_deactivate(id);
}

static int scheduler_enter_task_space(uint32_t task_id) {
    struct nova_task_info task;
    struct nova_address_space_info next_space, old_space;
    uint32_t next_id, old_id = active_address_space_id;
    uintptr_t next_root, old_root = nova_mmu_kernel_root();
    uint16_t next_asid, old_asid = 0U;

    if (!nova_task_get(task_id, &task)) return 0;
    next_id = task.address_space_id;
    if (next_id == old_id) return 1;

    if (old_id != 0U) {
        if (!nova_address_space_get(old_id, &old_space))
            scheduler_wait_forever();
        old_root = old_space.root_physical;
        old_asid = old_space.asid;
    }

    if (next_id == 0U) {
        next_root = nova_mmu_kernel_root();
        next_asid = 0U;
    } else {
        if (!nova_address_space_get(next_id, &next_space) ||
            !nova_sched_address_space_activate(next_id))
            return 0;
        next_root = next_space.root_physical;
        next_asid = next_space.asid;
    }

    if (!nova_mmu_switch_address_space(next_root, next_asid)) {
        if (next_id != 0U &&
            !nova_sched_address_space_deactivate(next_id))
            scheduler_wait_forever();
        return 0;
    }

    /*
     * The new TTBR0 is live, but ownership is not committed until the old
     * active reference is released. If that release fails, restore the old
     * translation regime before dropping the new reference.
     */
    if (old_id != 0U && !nova_sched_address_space_deactivate(old_id)) {
        if (!nova_mmu_switch_address_space(old_root, old_asid))
            scheduler_wait_forever();
        if (next_id != 0U &&
            !nova_sched_address_space_deactivate(next_id))
            scheduler_wait_forever();
        return 0;
    }

    active_address_space_id = next_id;
    return 1;
}
static int scheduler_return_to_kernel_space(void) {
    struct nova_address_space_info old_space;
    uint32_t old_id = active_address_space_id;

    /*
     * Preserve the old translation regime until its active reference has
     * been released. If release fails after TTBR0 changes, restore the old
     * root and retain the old ownership record.
     */
    if (old_id != 0U) {
        if (!nova_address_space_get(old_id, &old_space))
            scheduler_wait_forever();
    }

    if (!nova_mmu_switch_address_space(nova_mmu_kernel_root(), 0U))
        return 0;

    if (old_id != 0U && !nova_address_space_deactivate(old_id)) {
        if (!nova_mmu_switch_address_space(old_space.root_physical,
                                           old_space.asid))
            scheduler_wait_forever();
        return 0;
    }

    active_address_space_id = 0U;
    return 1;
}

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
        if (next_info.resume_kind == NOVA_TASK_RESUME_IRQ ||
            next_info.resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN) {
            struct nova_irq_frame *frame = 0;
            struct nova_cpu_context *first_user_context =
                next_info.resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN
                    ? nova_task_context(next) : 0;
            if (!scheduler_enter_task_space(next) ||
                !nova_task_take_irq_context(next, &frame)) {
                if (!scheduler_return_to_kernel_space()) {
                    irq_restore(saved_daif);
                    return 0;
                }
                irq_restore(saved_daif);
                continue;
            }

            current_task_id = next;
            next_after_id = next;
            current_quantum_ticks = 0U;
            preemption_pending = 0;
            execution_phase = NOVA_SCHED_TASK;

            if (next_info.resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN) {
                /*
                 * A first-run user task must have a prepared kernel context;
                 * never fall back to ERET without saving scheduler_context.
                 */
                if (first_user_context == 0) scheduler_wait_forever();
                nova_arch_context_switch(&scheduler_context,
                                         first_user_context);
            } else {
                nova_arch_irq_resume(frame);
            }

            execution_phase = NOVA_SCHED_SCHEDULER;
            if (!scheduler_return_to_kernel_space()) {
                irq_restore(saved_daif);
                return 0;
            }
            {
                uint32_t returned_id = current_task_id;
                struct nova_task_info returned_task;
                if (returned_id != 0U &&
                    nova_task_get(returned_id, &returned_task) &&
                    returned_task.state == NOVA_TASK_ZOMBIE)
                    (void)nova_task_destroy(returned_id);
            }
            current_task_id = 0U;
            irq_restore(saved_daif);
            continue;
        }

        {
            struct nova_cpu_context *context = nova_task_context(next);
            if (context == 0 || !scheduler_enter_task_space(next) ||
                !nova_task_transition(next, NOVA_TASK_RUNNING)) {
                if (!scheduler_return_to_kernel_space()) {
                    irq_restore(saved_daif);
                    return 0;
                }
                irq_restore(saved_daif);
                continue;
            }
            if (!nova_task_set_resume_kind(next, NOVA_TASK_RESUME_ACTIVE)) {
                (void)nova_task_transition(next, NOVA_TASK_READY);
                if (!scheduler_return_to_kernel_space()) {
                    irq_restore(saved_daif);
                    return 0;
                }
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
        if (!scheduler_return_to_kernel_space()) {
            irq_restore(saved_daif);
            return 0;
        }
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
    struct nova_task_info info;
    uintptr_t saved_daif = irq_save_disable();
    int result = 0;

    /*
     * An IRQ-preempted waiter is already READY when another task attempts
     * to wake it. Treat that wakeup as successful and idempotent.
     */
    if (nova_task_get(task_id, &info)) {
        if (info.state == NOVA_TASK_READY)
            result = 1;
        else if (info.state == NOVA_TASK_BLOCKED)
            result = nova_task_transition(task_id, NOVA_TASK_READY);
    }
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
    uint32_t candidate_id = 0U;
    uint32_t target_space_id = 0U;
    struct nova_task_info current_info, candidate_info;
    struct nova_address_space_info old_space, target_space;
    uintptr_t old_root = nova_mmu_kernel_root();
    uint16_t old_asid = 0U;
    uintptr_t target_root = 0U;
    uint16_t target_asid = 0U;
    int switched_space = 0;
    int target_activated = 0;
    int old_deactivated = 0;
    int switched;

    if (execution_phase != NOVA_SCHED_TASK || current_id == 0U ||
        !preemption_pending || frame == 0 || next_id == 0 || next_frame == 0) {
        irq_restore(saved_daif);
        return 0;
    }

    /*
     * The current task's address space must match the root currently active
     * on this CPU. IRQs are masked, so the selected peer cannot change between
     * this preview and nova_task_irq_handoff().
     */
    if (!nova_task_get(current_id, &current_info) ||
        current_info.address_space_id != active_address_space_id) {
        current_quantum_ticks = 0U;
        preemption_pending = 0;
        irq_restore(saved_daif);
        return 0;
    }

    if (active_address_space_id != 0U) {
        if (!nova_address_space_get(active_address_space_id, &old_space))
            scheduler_wait_forever();
        old_root = old_space.root_physical;
        old_asid = old_space.asid;
    }

    if (nova_task_next_irq_ready(current_id, &candidate_id) &&
        candidate_id != current_id) {
        if (!nova_task_get(candidate_id, &candidate_info)) {
            current_quantum_ticks = 0U;
            preemption_pending = 0;
            irq_restore(saved_daif);
            return 0;
        }
        target_space_id = candidate_info.address_space_id;

        if (target_space_id != active_address_space_id) {
            if (target_space_id == 0U) {
                target_root = nova_mmu_kernel_root();
                target_asid = 0U;
            } else {
                if (!nova_address_space_get(target_space_id, &target_space) ||
                    !nova_sched_address_space_activate(target_space_id)) {
                    current_quantum_ticks = 0U;
                    preemption_pending = 0;
                    irq_restore(saved_daif);
                    return 0;
                }
                target_activated = 1;
                target_root = target_space.root_physical;
                target_asid = target_space.asid;
            }

            /*
             * Keep both spaces pinned across the TTBR0 transition. Do not
             * commit task selection until the old active reference is safely
             * released; otherwise a failed release could leave bookkeeping
             * inconsistent with the active translation regime.
             */
            if (!nova_mmu_switch_address_space(target_root, target_asid)) {
                if (target_activated &&
                    !nova_sched_address_space_deactivate(target_space_id))
                    scheduler_wait_forever();
                current_quantum_ticks = 0U;
                preemption_pending = 0;
                irq_restore(saved_daif);
                return 0;
            }
            switched_space = 1;

            if (active_address_space_id != 0U) {
                if (!nova_sched_address_space_deactivate(active_address_space_id)) {
                    if (!nova_mmu_switch_address_space(old_root, old_asid))
                        scheduler_wait_forever();
                    if (target_activated &&
                        !nova_sched_address_space_deactivate(target_space_id))
                        scheduler_wait_forever();
                    current_quantum_ticks = 0U;
                    preemption_pending = 0;
                    irq_restore(saved_daif);
                    return 0;
                }
                old_deactivated = 1;
            }
        }
    }

    switched = nova_task_irq_handoff(current_id, frame, next_id, next_frame);
    current_quantum_ticks = 0U;
    preemption_pending = 0U;

    if (switched) {
        if (switched_space)
            active_address_space_id = target_space_id;
        current_task_id = *next_id;
        next_after_id = *next_id;
    } else if (switched_space) {
        /*
         * The task-layer handoff declined the switch. Restore the old active
         * reference before reloading its TTBR0, then release the destination.
         */
        if (old_deactivated &&
            !nova_sched_address_space_activate(current_info.address_space_id))
            scheduler_wait_forever();
        if (!nova_mmu_switch_address_space(old_root, old_asid))
            scheduler_wait_forever();
        if (target_activated &&
            !nova_sched_address_space_deactivate(target_space_id))
            scheduler_wait_forever();
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
