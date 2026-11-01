#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "context.h"
#include "scheduler.h"
#include "task.h"
#include "address_space.h"

static _Alignas(16) unsigned char stack_pool[NOVA_TASK_MAX][4096];
static size_t next_stack;
static unsigned int switch_depth;
static uint32_t observed_task;
static uint32_t peer_task;
static int irq_resume_mock;
static uint32_t user_task;
static uint32_t user_space_id;
static uint32_t peer_space_id;
static uint32_t failure_space_id;
static int user_space_mode;
static int failure_mode;
static uintptr_t mock_ttbr_root = 0x1000U;
static uint16_t mock_ttbr_asid;
static int fail_kernel_space_switch;

void nova_task_start_trampoline(void) {}

uintptr_t nova_mmu_kernel_root(void) { return 0x1000U; }
int nova_mmu_switch_address_space(uintptr_t root, uint16_t asid) {
    if (fail_kernel_space_switch && root == 0x1000U && asid == 0U)
        return 0;
    mock_ttbr_root = root;
    mock_ttbr_asid = asid;
    return 1;
}


void *nova_vmap_alloc_page(unsigned int flags) {
    (void)flags;
    if (next_stack >= NOVA_TASK_MAX) return 0;
    return stack_pool[next_stack++];
}

int nova_vmap_free_page(void *address) {
    (void)address;
    return 1;
}
void *nova_vmap_alloc_guarded_page(unsigned int flags) {
    return nova_vmap_alloc_page(flags);
}
int nova_vmap_free_guarded_page(void *address) {
    return nova_vmap_free_page(address);
}

void nova_arch_context_switch(struct nova_cpu_context *from,
                             const struct nova_cpu_context *to) {
    struct nova_task_info current_info;
    (void)from;
    (void)to;
    if (user_space_mode) {
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
        assert(nova_sched_current() == user_task);
        return;
    }
    if (failure_mode) {
        if (nova_sched_execution_phase() == NOVA_SCHED_TASK) {
            assert(nova_sched_current() == user_task);
            failure_mode = 2;
            assert(nova_sched_block_current());
            return;
        }
        assert(failure_mode == 2);
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
        assert(nova_sched_current() == user_task);
        return;
    }
    if (irq_resume_mock) {
        assert(nova_sched_execution_phase() == NOVA_SCHED_SCHEDULER);
        assert(nova_sched_current() == observed_task);
        return;
    }
    if (switch_depth == 0U) {
        unsigned int i;
        assert(nova_sched_execution_phase() == NOVA_SCHED_TASK);
        observed_task = nova_sched_current();
        assert(nova_task_get(observed_task, &current_info));
        if (current_info.address_space_id == user_space_id) {
            struct nova_address_space_info active_space;
            assert(nova_address_space_get(user_space_id, &active_space));
            assert(mock_ttbr_root == active_space.root_physical);
            assert(mock_ttbr_asid == active_space.asid);
            assert(active_space.active_cpus == 1U);
        } else if (current_info.address_space_id == peer_space_id ||
                   current_info.address_space_id == failure_space_id) {
            struct nova_address_space_info active_space;
            assert(nova_address_space_get(peer_space_id, &active_space));
            assert(mock_ttbr_root == active_space.root_physical);
            assert(mock_ttbr_asid == active_space.asid);
            assert(active_space.active_cpus == 1U);
        } else {
            assert(0 && "unexpected task address space");
        }
        assert(observed_task != 0U);
        if (observed_task == user_task) {
            struct nova_address_space_info space;
            assert(nova_address_space_get(user_space_id, &space));
            assert(space.active_cpus == 1U);
            user_space_mode = 1;
            assert(nova_sched_block_current());
            user_space_mode = 0;
            return;
        }
        for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
        assert(nova_sched_preemption_pending());
        {
            struct nova_task_info peer_info;
            struct nova_irq_frame *selected_frame = 0;
            uint32_t selected_id = 0U;
            assert(nova_task_get(observed_task, &current_info));
            assert(current_info.resume_kind == NOVA_TASK_RESUME_ACTIVE);
            assert(nova_task_get(peer_task, &peer_info));
            ((struct nova_irq_frame *)(current_info.kernel_stack_top -
                sizeof(struct nova_irq_frame)))->sp_el0 = 0x70001000U;
            peer_info.irq_frame->sp_el0 = 0x70002000U;
            /* Reject a malformed IRQ frame without consuming the quantum. */
            assert(!nova_sched_irq_handoff(0, &selected_id, &selected_frame));
            assert(nova_sched_preemption_pending());
            assert(nova_sched_current() == observed_task);
            assert(nova_sched_irq_handoff(
                                          (struct nova_irq_frame *)(current_info.kernel_stack_top -
                                              sizeof(struct nova_irq_frame)),
                                          &selected_id, &selected_frame));
            assert(mock_ttbr_root == 0x3000U && mock_ttbr_asid == 8U);
            assert(selected_id == peer_task);
            assert(selected_frame == peer_info.irq_frame);
            assert(selected_frame->sp_el0 == 0x70002000U);
            assert(nova_sched_current() == peer_task);
            assert(nova_task_get(observed_task, &current_info));
            assert(current_info.state == NOVA_TASK_READY &&
                   current_info.resume_kind == NOVA_TASK_RESUME_IRQ);
            /* A waiter preempted into READY is already successfully awake. */
            assert(nova_sched_unblock(observed_task));
            assert(nova_task_get(observed_task, &current_info));
            assert(current_info.state == NOVA_TASK_READY &&
                   current_info.resume_kind == NOVA_TASK_RESUME_IRQ);

            for (i = 0; i < 5U; ++i) nova_sched_timer_tick();
            assert(nova_sched_preemption_pending());
            assert(nova_sched_irq_handoff(
                                          (struct nova_irq_frame *)(peer_info.kernel_stack_top -
                                              sizeof(struct nova_irq_frame)),
                                          &selected_id, &selected_frame));
            assert(mock_ttbr_root == 0x2000U && mock_ttbr_asid == 7U);
            assert(selected_id == observed_task);
            assert(selected_frame == current_info.irq_frame);
            assert(selected_frame->sp_el0 == 0x70001000U);
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
            assert(mock_ttbr_root == 0x3000U && mock_ttbr_asid == 8U);
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

void nova_arch_irq_resume(const struct nova_irq_frame *frame) {
    assert(frame != 0);
    assert(nova_sched_execution_phase() == NOVA_SCHED_TASK);
    assert(nova_sched_current() == observed_task);
    irq_resume_mock = 1;
    assert(nova_sched_block_current());
    irq_resume_mock = 0;
}

static void dummy_entry(void *argument) { (void)argument; }

int main(void) {
    uint32_t task_id;
    struct nova_task_info info;
    struct nova_address_space_info space;

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

    assert(nova_address_space_create(0x2000U, 7U, &user_space_id));
    assert(nova_address_space_create(0x3000U, 8U, &peer_space_id));
    assert(nova_task_create(dummy_entry, 0, &task_id));
    assert(nova_task_create(dummy_entry, 0, &peer_task));
    assert(nova_task_set_address_space(task_id, user_space_id));
    assert(nova_task_set_address_space(peer_task, peer_space_id));
    assert(nova_sched_start());
    assert(mock_ttbr_root == 0x1000U && mock_ttbr_asid == 0U);
    assert(nova_address_space_get(user_space_id, &space));
    assert(space.references == 2U && space.active_cpus == 0U);
    assert(nova_address_space_get(peer_space_id, &space));
    assert(space.references == 2U && space.active_cpus == 0U);
    assert(nova_sched_execution_phase() == NOVA_SCHED_INACTIVE);
    assert(nova_sched_current() == 0U);
    assert(observed_task == task_id);
    assert(nova_task_get(task_id, &info));
    assert(info.state == NOVA_TASK_BLOCKED &&
           info.resume_kind == NOVA_TASK_RESUME_COOPERATIVE);
    assert(!nova_sched_preemption_pending());
    assert(nova_task_get(peer_task, &info));
    assert(info.state == NOVA_TASK_BLOCKED &&
           info.resume_kind == NOVA_TASK_RESUME_COOPERATIVE);
    assert(nova_task_create(dummy_entry, 0, &user_task));
    assert(nova_task_set_address_space(user_task, user_space_id));
    assert(nova_sched_start());
    assert(mock_ttbr_root == 0x1000U && mock_ttbr_asid == 0U);
    assert(nova_address_space_get(user_space_id, &space));
    assert(space.references == 3U && space.active_cpus == 0U);
    assert(nova_address_space_get(peer_space_id, &space));
    assert(space.references == 2U && space.active_cpus == 0U);
    assert(nova_task_get(user_task, &info));
    assert(info.state == NOVA_TASK_BLOCKED && info.address_space_id == user_space_id);
    assert(nova_task_transition(user_task, NOVA_TASK_READY));
    assert(nova_task_transition(user_task, NOVA_TASK_RUNNING));
    assert(nova_task_transition(user_task, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(user_task));
    assert(nova_address_space_get(user_space_id, &space));
    assert(space.references == 2U && space.active_cpus == 0U);

    assert(nova_sched_unblock(task_id));
    assert(nova_task_transition(task_id, NOVA_TASK_RUNNING));
    assert(nova_task_transition(task_id, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(task_id));
    assert(nova_sched_unblock(peer_task));
    assert(nova_task_transition(peer_task, NOVA_TASK_RUNNING));
    assert(nova_task_transition(peer_task, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(peer_task));
    assert(nova_address_space_get(user_space_id, &space));
    assert(space.references == 1U && space.active_cpus == 0U);
    assert(nova_address_space_get(peer_space_id, &space));
    assert(space.references == 1U && space.active_cpus == 0U);
    assert(nova_address_space_release(user_space_id));
    assert(nova_address_space_release(peer_space_id));

    /*
     * If the return-to-kernel TTBR0 switch fails, the scheduler must not reap
     * the blocked task or drop its active address-space reference.
     */
    {
        struct nova_address_space_info failure_info;
        assert(nova_address_space_create(0x4000U, 9U, &failure_space_id));
        assert(nova_task_create(dummy_entry, 0, &user_task));
        assert(nova_task_set_address_space(user_task, failure_space_id));
        fail_kernel_space_switch = 1;
        failure_mode = 1;
        assert(!nova_sched_start());
        assert(nova_task_get(user_task, &info));
        assert(info.state == NOVA_TASK_BLOCKED);
        assert(nova_sched_current() == user_task);
        assert(nova_address_space_get(failure_space_id, &failure_info));
        assert(failure_info.references == 2U && failure_info.active_cpus == 1U);
        assert(mock_ttbr_root == 0x4000U && mock_ttbr_asid == 9U);
    }
    puts("Scheduler address-space rollback failure containment tests passed.");
    return 0;
}
