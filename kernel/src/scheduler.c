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
__attribute__((weak)) uintptr_t nova_mmu_kernel_root(void) { return 0U; }
__attribute__((weak)) int nova_mmu_switch_address_space(uintptr_t root, uint16_t asid) {
    (void)root; (void)asid; return 1;
}
static int scheduler_enter_task_space(uint32_t task_id) {
    struct nova_task_info task;
    struct nova_address_space_info space;
    uint32_t next_id; uintptr_t root; uint16_t asid;
    if (!nova_task_get(task_id, &task)) return 0;
    next_id=task.address_space_id;
    if (next_id==active_address_space_id) return 1;
    if (next_id==0U) { root=nova_mmu_kernel_root(); asid=0U; }
    else {
        if (!nova_address_space_get(next_id,&space) ||
            !nova_address_space_activate(next_id)) return 0;
        root=space.root_physical; asid=space.asid;
    }
    if (!nova_mmu_switch_address_space(root,asid)) {
        if(next_id!=0U)(void)nova_address_space_deactivate(next_id);
        return 0;
    }
    if(active_address_space_id!=0U)(void)nova_address_space_deactivate(active_address_space_id);
    active_address_space_id=next_id; return 1;
}
static void scheduler_return_to_kernel_space(void) {
    if(!nova_mmu_switch_address_space(nova_mmu_kernel_root(),0U)) return;
    if(active_address_space_id!=0U)(void)nova_address_space_deactivate(active_address_space_id);
    active_address_space_id=0U;
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
        if (next_info.resume_kind == NOVA_TASK_RESUME_IRQ) {
            struct nova_irq_frame *frame = 0;
            if (!scheduler_enter_task_space(next) ||
                !nova_task_take_irq_context(next, &frame)) {
                scheduler_return_to_kernel_space();
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
            /*
             * Only a returning host test shim reaches this path. On hardware,
             * ERET resumes the task and a later cooperative switch returns to
             * the continuation following the original context switch below.
             */
            execution_phase = NOVA_SCHED_SCHEDULER;
            scheduler_return_to_kernel_space();
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
                scheduler_return_to_kernel_space();
                irq_restore(saved_daif);
                continue;
            }
            if (!nova_task_set_resume_kind(next, NOVA_TASK_RESUME_ACTIVE)) {
                (void)nova_task_transition(next, NOVA_TASK_READY);
                scheduler_return_to_kernel_space();
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
        scheduler_return_to_kernel_space();
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
    struct nova_address_space_info target_space;
    uintptr_t target_root = 0U;
    uint16_t target_asid = 0U;
    int switched_space = 0;
    int target_activated = 0;
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
                    !nova_address_space_activate(target_space_id)) {
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
             * Activate the destination before changing TTBR0. The current
             * kernel stack and IRQ frame remain reachable through the shared
             * privileged mappings during this short handoff window.
             */
            if (!nova_mmu_switch_address_space(target_root, target_asid)) {
                if (target_activated)
                    (void)nova_address_space_deactivate(target_space_id);
                current_quantum_ticks = 0U;
                preemption_pending = 0;
                irq_restore(saved_daif);
                return 0;
            }
            switched_space = 1;
        }
    }

    switched = nova_task_irq_handoff(current_id, frame, next_id, next_frame);
    current_quantum_ticks = 0U;
    preemption_pending = 0;

    if (switched) {
        if (switched_space) {
            if (active_address_space_id != 0U)
                (void)nova_address_space_deactivate(active_address_space_id);
            active_address_space_id = target_space_id;
        }
        current_task_id = *next_id;
        next_after_id = *next_id;
    } else if (switched_space) {
        uintptr_t old_root = nova_mmu_kernel_root();
        uint16_t old_asid = 0U;
        if (current_info.address_space_id != 0U) {
            struct nova_address_space_info old_space;
            if (!nova_address_space_get(current_info.address_space_id,
                                        &old_space))
                for (;;) __asm__ volatile("wfe");
            old_root = old_space.root_physical;
            old_asid = old_space.asid;
        }
        if (!nova_mmu_switch_address_space(old_root, old_asid))
            for (;;) __asm__ volatile("wfe");
        if (target_activated)
            (void)nova_address_space_deactivate(target_space_id);
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
