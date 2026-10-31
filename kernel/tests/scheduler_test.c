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
static uint32_t peer_task;

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
    struct nova_task_info current_info;
    (void)from;
    (void)to;
    if (switch_depth == 0U) {
        unsigned int i;
        assert(nova_sched_execution_phase() == NOVA_SCHED_TASK);
        observed_task = nova_sched_current();
        assert(observed_task != 0U);
        for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
        assert(nova_sched_preemption_pending());
        {
            struct nova_task_info peer_info;
            struct nova_irq_frame *selected_frame = 0;
            uint32_t selected_id = 0U;
            assert(nova_task_get(observed_task, &current_info));
            assert(current_info.resume_kind == NOVA_TASK_RESUME_ACTIVE);
            assert(nova_task_get(peer_task, &peer_info));
            /* Reject a malformed IRQ frame without consuming the quantum. */
            assert(!nova_sched_irq_handoff(0, &selected_id, &selected_frame));
            assert(nova_sched_preemption_pending());
            assert(nova_sched_current() == observed_task);
            assert(nova_sched_irq_handoff(
                                          (struct nova_irq_frame *)(current_info.kernel_stack_top -
                                              sizeof(struct nova_irq_frame)),
                                          &selected_id, &selected_frame));
            assert(selected_id == peer_task);
            assert(selected_frame == peer_info.irq_frame);
            assert(nova_sched_current() == peer_task);
            assert(nova_task_get(observed_task, &current_info));
            assert(current_info.state == NOVA_TASK_READY &&
                   current_info.resume_kind == NOVA_TASK_RESUME_IRQ);

            for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
            assert(nova_sched_preemption_pending());
            assert(nova_sched_irq_handoff(
                                          (struct nova_irq_frame *)(peer_info.kernel_stack_top -
                                              sizeof(struct nova_irq_frame)),
                                          &selected_id, &selected_frame));
            assert(selected_id == observed_task);
            assert(selected_frame == current_info.irq_frame);
            assert(nova_sched_current() == observed_task);

            /* An expired quantum with no IRQ-resumable peer keeps this frame. */
            assert(nova_task_transition(peer_task, NOVA_TASK_RUNNING));
            assert(nova_task_transition(peer_task, NOVA_TASK_BLOCKED));
            for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
            assert(nova_sched_preemption_pending());
            assert(!nova_sched_irq_handoff(
                                           (struct nova_irq_frame *)(current_info.kernel_stack_top -
                                               sizeof(struct nova_irq_frame)),
                                           &selected_id, &selected_frame));
            assert(selected_id == observed_task);
            assert(selected_frame ==
                   (struct nova_irq_frame *)(current_info.kernel_stack_top -
                       sizeof(struct nova_irq_frame)));
            assert(nova_sched_current() == observed_task);
            assert(!nova_sched_preemption_pending());
            assert(nova_task_transition(peer_task, NOVA_TASK_READY));
            /*
             * B's earlier IRQ frame was consumed when B ran. Before using B
             * as an IRQ-resumable peer again, model a fresh interrupt frame.
             */
            assert(nova_task_transition(peer_task, NOVA_TASK_RUNNING));
            assert(nova_task_mark_irq_resumable(
                peer_task,
                (struct nova_irq_frame *)(peer_info.kernel_stack_top -
                    sizeof(struct nova_irq_frame))));
            assert(nova_task_transition(peer_task, NOVA_TASK_READY));
        }
        /*
         * Switch A -> B through an IRQ frame, then simulate B yielding.
         * The shared scheduler continuation must account for B, not A.
         */
        {
            struct nova_irq_frame *selected_frame = 0;
            uint32_t selected_id = 0U;
            for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
            assert(nova_sched_preemption_pending());
            assert(nova_sched_irq_handoff(
                (struct nova_irq_frame *)(current_info.kernel_stack_top -
                    sizeof(struct nova_irq_frame)),
                &selected_id, &selected_frame));
            assert(selected_id == peer_task);
            assert(nova_sched_current() == peer_task);
        }
        ++switch_depth;
        assert(nova_sched_block_current());
        --switch_depth;
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
        assert(nova_sched_current() == peer_task);
        nova_sched_timer_tick();
        assert(!nova_sched_preemption_pending());
    } else {
        struct nova_irq_frame *selected_frame = 0;
        uint32_t selected_id = 0U;
        assert(switch_depth == 1U);
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
        /* A scheduler-stack IRQ must never be attached to a task. */
        assert(!nova_sched_irq_handoff((struct nova_irq_frame *)to,
                                       &selected_id, &selected_frame));
        assert(nova_sched_current() == peer_task);
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
    {
        _Alignas(16) struct nova_irq_frame inactive_frame;
        struct nova_irq_frame *selected_frame = 0;
        uint32_t selected_id = 0U;
        assert(!nova_sched_irq_handoff(&inactive_frame, &selected_id,
                                       &selected_frame));
        assert(nova_sched_current() == 0U);
    }

    assert(nova_task_create(dummy_entry, 0, &task_id));
    assert(nova_task_create(dummy_entry, 0, &peer_task));
    assert(nova_sched_start());
    assert(nova_sched_execution_phase() == NOVA_SCHED_INACTIVE);
    assert(nova_sched_current() == 0U);
    assert(observed_task == task_id);
    assert(nova_task_get(task_id, &info));
    assert(info.state == NOVA_TASK_READY &&
           info.resume_kind == NOVA_TASK_RESUME_IRQ);
    assert(!nova_sched_preemption_pending());
    assert(nova_task_get(peer_task, &info));
    assert(info.state == NOVA_TASK_BLOCKED &&
           info.resume_kind == NOVA_TASK_RESUME_COOPERATIVE);

    assert(nova_sched_unblock(task_id));
    assert(nova_task_transition(task_id, NOVA_TASK_RUNNING));
    assert(nova_task_transition(task_id, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(task_id));
    assert(nova_task_transition(peer_task, NOVA_TASK_RUNNING));
    assert(nova_task_transition(peer_task, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(peer_task));
    puts("Scheduler phase and cooperative handoff host tests passed.");
    return 0;
}
