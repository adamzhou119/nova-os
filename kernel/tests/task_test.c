#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "task.h"
#include "context.h"
#include "vm.h"
#include "address_space.h"

static _Alignas(16) unsigned char stack_pool[16][4096];
static size_t next_stack;
static int freed_stacks;

void nova_task_start_trampoline(void) {}

void *nova_vmap_alloc_page(unsigned int flags) {
    void *result;
    (void)flags;
    if (next_stack >= 16U) return 0;
    result = stack_pool[next_stack++];
    return result;
}
int nova_vmap_free_page(void *address) {
    (void)address;
    ++freed_stacks;
    return 1;
}
void *nova_vmap_alloc_guarded_page(unsigned int flags) {
    return nova_vmap_alloc_page(flags);
}
int nova_vmap_free_guarded_page(void *address) {
    return nova_vmap_free_page(address);
}
static void dummy_entry(void *argument) { (void)argument; }

int main(void) {
    uint32_t first, second, address_space_id, user_space_id, user_first;
    struct nova_address_space_info address_space_info;
    struct nova_task_info info;
    struct nova_cpu_context context;
    struct nova_cpu_context *task_context;
    int argument = 42;
    assert(!nova_arch_context_init(&context, 0U, 0x2000U, 1U, 2U));
    assert(!nova_arch_context_init(&context, 0x1001U, 0x2000U, 1U, 2U));
    assert(nova_arch_context_init(&context, 0x3000U, 0x2000U, 1U, 2U));
    assert(context.sp == 0x3000U);
    assert(context.x19_x30[0] == 1U && context.x19_x30[1] == 2U);
    assert(context.x19_x30[11] == 0x2000U);
    assert(!nova_task_create(0, &argument, &first));
    assert(!nova_task_create(dummy_entry, &argument, 0));
    assert(nova_task_create(dummy_entry, &argument, &first));
    assert(nova_task_create(dummy_entry, 0, &second));
    assert(first != second);
    assert(nova_address_space_create(0x40008000U, 1U, &address_space_id));
    assert(nova_task_set_address_space(first, address_space_id));
    assert(!nova_task_set_address_space(first, 999U));
    assert(nova_address_space_get(address_space_id, &address_space_info));
    assert(address_space_info.references == 2U);
    assert(nova_address_space_release(address_space_id));
    assert(nova_task_get(first, &info));
    assert(info.address_space_id == address_space_id);
    assert(!nova_task_set_resume_kind(first, NOVA_TASK_RESUME_FIRST_RUN));

    /* Ready selection must be round-robin by task ID and skip blocked tasks. */
    {
        uint32_t selected = 0U;
        assert(nova_task_next_ready(0U, &selected) && selected == first);
        assert(nova_task_next_ready(first, &selected) && selected == second);
        assert(nova_task_next_ready(second, &selected) && selected == first);
        assert(nova_task_transition(first, NOVA_TASK_RUNNING));
        assert(nova_task_transition(first, NOVA_TASK_BLOCKED));
        assert(nova_task_next_ready(0U, &selected) && selected == second);
        assert(nova_task_transition(first, NOVA_TASK_READY));
        assert(nova_task_next_ready(second, &selected) && selected == first);
        assert(!nova_task_next_ready(0U, 0));
    }
    assert(nova_task_get(first, &info));
    task_context = nova_task_context(first);
    assert(task_context != 0);
    assert(task_context->sp == (uintptr_t)info.irq_frame);
    assert(info.irq_frame != 0);
    assert((uintptr_t)info.irq_frame == info.kernel_stack_top - sizeof(struct nova_irq_frame));
    assert(info.irq_frame->x[0] == (uintptr_t)&argument);
    assert(info.irq_frame->x[19] == (uintptr_t)dummy_entry);
    assert(info.irq_frame->x[20] == (uintptr_t)&argument);
    assert(info.irq_frame->x[21] != 0U);
    assert(info.irq_frame->elr_el1 == (uintptr_t)nova_task_start_trampoline);
    assert(info.irq_frame->spsr_el1 == 0x5U);
    /* IRQ entry may replace the initial frame with a live frame on this stack. */
    {
        struct nova_irq_frame *live_frame =
            (struct nova_irq_frame *)(info.kernel_stack_top - 2U * sizeof(struct nova_irq_frame));
        assert(nova_task_set_irq_frame(first, live_frame));
        assert(nova_task_get(first, &info) && info.irq_frame == live_frame);
        assert(!nova_task_set_irq_frame(first,
            (struct nova_irq_frame *)(info.kernel_stack_base - 16U)));
        assert(!nova_task_set_irq_frame(first,
            (struct nova_irq_frame *)((uintptr_t)live_frame + 8U)));
        assert(nova_task_set_irq_frame(first,
            (struct nova_irq_frame *)(info.kernel_stack_top - sizeof(struct nova_irq_frame))));
    }
    assert(task_context->x19_x30[0] == (uintptr_t)dummy_entry);
    assert(task_context->x19_x30[1] == (uintptr_t)&argument);
    assert(task_context->x19_x30[2] != 0U);
    assert(task_context->x19_x30[11] == (uintptr_t)nova_task_start_trampoline);
    assert(info.state == NOVA_TASK_READY);
    assert(info.resume_kind == NOVA_TASK_RESUME_FIRST_RUN);
    assert(info.entry == dummy_entry && info.argument == &argument);
    assert(info.kernel_stack_top - info.kernel_stack_base == 4096U);
    assert((info.kernel_stack_top & 15U) == 0U);
    assert(!nova_task_transition(first, NOVA_TASK_BLOCKED));
    /* IRQ frames are a distinct resume path, never a cooperative context. */
    assert(nova_task_transition(first, NOVA_TASK_RUNNING));
    assert(!nova_task_set_address_space(first, 0U));
    assert(!nova_task_set_resume_kind(first, NOVA_TASK_RESUME_COOPERATIVE));
    /* Invalid frame publication must leave ownership and state untouched. */
    assert(!nova_task_save_irq_context(first,
        (struct nova_irq_frame *)(info.kernel_stack_base - 16U)));
    assert(nova_task_get(first, &info));
    assert(info.state == NOVA_TASK_RUNNING);
    assert(info.resume_kind == NOVA_TASK_RESUME_FIRST_RUN);
    assert(nova_task_save_irq_context(first, info.irq_frame));
    assert(nova_task_get(first, &info));
    assert(info.resume_kind == NOVA_TASK_RESUME_IRQ);
    assert(info.state == NOVA_TASK_READY);
    {
        uint32_t selected = 0U;
        assert(nova_task_next_ready(0U, &selected) && selected == first);
        assert(nova_task_next_cooperative_ready(0U, &selected) && selected == second);
        /* IRQ return may resume FIRST_RUN or saved IRQ frames, never COOP. */
        assert(nova_task_next_irq_ready(0U, &selected) && selected == first);
        assert(nova_task_next_irq_ready(first, &selected) && selected == second);
        assert(nova_task_set_resume_kind(first, NOVA_TASK_RESUME_COOPERATIVE));
        assert(nova_task_next_cooperative_ready(0U, &selected) && selected == first);
        assert(nova_task_next_irq_ready(0U, &selected) && selected == second);
            /* Atomic IRQ handoff rotates only between IRQ-returnable frames. */
        {
            struct nova_irq_frame *chosen_frame = 0;
            uint32_t chosen_id = 0U;
            assert(nova_task_transition(first, NOVA_TASK_RUNNING));
            assert(nova_task_irq_handoff(first, info.irq_frame,
                                         &chosen_id, &chosen_frame));
            assert(chosen_id == second && chosen_frame != 0);
            assert(nova_task_get(first, &info) &&
                   info.state == NOVA_TASK_READY &&
                   info.resume_kind == NOVA_TASK_RESUME_IRQ);
            assert(nova_task_get(second, &info) &&
                   info.state == NOVA_TASK_RUNNING &&
                   info.resume_kind == NOVA_TASK_RESUME_ACTIVE &&
                   info.irq_frame == 0);
            assert(nova_task_irq_handoff(second,
                                         (struct nova_irq_frame *)(info.kernel_stack_top -
                                             sizeof(struct nova_irq_frame)),
                                         &chosen_id, &chosen_frame));
            assert(chosen_id == first && chosen_frame != 0);
            assert(nova_task_get(second, &info) &&
                   info.state == NOVA_TASK_READY);
            assert(nova_task_get(first, &info) &&
                   info.state == NOVA_TASK_RUNNING &&
                   info.resume_kind == NOVA_TASK_RESUME_ACTIVE &&
                   info.irq_frame == 0);

            /* With no IRQ-resumable peer, ERET consumes the current frame. */
            assert(nova_task_transition(second, NOVA_TASK_RUNNING));
            assert(nova_task_transition(second, NOVA_TASK_BLOCKED));
            assert(nova_task_transition(first, NOVA_TASK_READY));
            assert(nova_task_set_resume_kind(first, NOVA_TASK_RESUME_COOPERATIVE));
            assert(nova_task_transition(first, NOVA_TASK_RUNNING));
            assert(nova_task_get(first, &info));
            {
                struct nova_irq_frame *live_frame =
                    (struct nova_irq_frame *)(info.kernel_stack_top -
                        sizeof(struct nova_irq_frame));
                assert(!nova_task_irq_handoff(first, live_frame,
                                              &chosen_id, &chosen_frame));
                assert(chosen_id == first && chosen_frame == live_frame);
                assert(nova_task_get(first, &info));
                assert(info.state == NOVA_TASK_RUNNING);
                assert(info.resume_kind == NOVA_TASK_RESUME_ACTIVE);
                assert(info.irq_frame == 0);
            }
            assert(nova_task_transition(second, NOVA_TASK_READY));
            /*
             * Scheduler dispatch consumes an IRQ frame exactly once and
             * converts the task back to ACTIVE before ERET.
             */
            assert(nova_task_transition(second, NOVA_TASK_RUNNING));
            assert(nova_task_get(second, &info));
            {
                struct nova_irq_frame *saved_frame =
                    (struct nova_irq_frame *)(info.kernel_stack_top -
                        sizeof(struct nova_irq_frame));
                struct nova_irq_frame *taken_frame = 0;
                assert(nova_task_mark_irq_resumable(second, saved_frame));
                assert(nova_task_transition(second, NOVA_TASK_READY));
                assert(nova_task_take_irq_context(second, &taken_frame));
                assert(taken_frame == saved_frame);
                assert(nova_task_get(second, &info));
                assert(info.state == NOVA_TASK_RUNNING);
                assert(info.resume_kind == NOVA_TASK_RESUME_ACTIVE);
                assert(info.irq_frame == 0);
                assert(!nova_task_take_irq_context(second, &taken_frame));
            }
        }
    assert(!nova_task_destroy(first));
    }
    assert(!nova_task_destroy(first));
    assert(nova_task_set_exit_code(first, -7));
    assert(nova_task_transition(first, NOVA_TASK_BLOCKED));
    assert(!nova_task_set_exit_code(first, 0));
    assert(nova_task_transition(first, NOVA_TASK_READY));
    assert(nova_task_transition(first, NOVA_TASK_RUNNING));
    assert(nova_task_transition(first, NOVA_TASK_ZOMBIE));
    assert(nova_task_get(first, &info) && info.exit_code == -7);
    assert(info.fault_kind == NOVA_TASK_FAULT_NONE);
    assert(nova_task_destroy(first));
    assert(!nova_address_space_get(address_space_id, &address_space_info));
    assert(!nova_task_get(first, &info));

    /* First-run EL0 tasks own a retained address-space reference and ERET frame. */
    assert(nova_address_space_create(0x50008000U, 2U, &user_space_id));
    assert(nova_task_create_user(user_space_id, 0x80004000U, 0x80010000U,
                                 0x1234U, &user_first));
    assert(nova_task_get(user_first, &info));
    assert(info.resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN);
    assert(info.state == NOVA_TASK_READY && info.address_space_id == user_space_id);
    assert(info.user_entry == 0x80004000U && info.user_stack_top == 0x80010000U);
    assert(info.entry == 0 && info.context.sp == info.kernel_stack_top);
    assert(info.context.x19_x30[0] == (uintptr_t)info.irq_frame);
    assert(info.context.x19_x30[11] != 0U);
    assert(info.irq_frame != 0 && info.irq_frame->elr_el1 == 0x80004000U);
    assert(info.irq_frame->spsr_el1 == 0U);
    assert(info.irq_frame->sp_el0 == 0x80010000U);
    assert(info.irq_frame->x[0] == 0x1234U);
    assert(info.fault_kind == NOVA_TASK_FAULT_NONE);
    assert(info.fault_esr == 0U && info.fault_far == 0U && info.fault_elr == 0U);
    assert(nova_address_space_get(user_space_id, &address_space_info));
    assert(address_space_info.references == 2U);
    assert(nova_address_space_release(user_space_id));
    {
        struct nova_irq_frame *taken = 0;
        uint32_t selected = 0U;
        assert(!nova_task_next_cooperative_ready(0U, &selected));
        assert(nova_task_next_irq_ready(0U, &selected) && selected == user_first);
        assert(nova_task_take_irq_context(user_first, &taken));
        assert(taken == info.irq_frame);
        assert(nova_task_get(user_first, &info));
        assert(info.state == NOVA_TASK_RUNNING &&
               info.resume_kind == NOVA_TASK_RESUME_ACTIVE);
        assert(!nova_task_take_irq_context(user_first, &taken));
    }
    assert(!nova_task_set_fault(user_first, NOVA_TASK_FAULT_NONE,
                                  0x90000000U, 0x80004000U, 0x80001008U));
    assert(!nova_task_set_fault(user_first,
                                (enum nova_task_fault_kind)99,
                                0x90000000U, 0x80004000U, 0x80001008U));
    assert(nova_task_set_fault(user_first, NOVA_TASK_FAULT_DATA_ABORT,
                               0x90000000U, 0x80004000U, 0x80001008U));
    assert(nova_task_get(user_first, &info));
    assert(info.exit_code == NOVA_TASK_EXIT_FAULT);
    assert(info.fault_kind == NOVA_TASK_FAULT_DATA_ABORT);
    assert(info.fault_esr == 0x90000000U &&
           info.fault_far == 0x80004000U &&
           info.fault_elr == 0x80001008U);
    assert(nova_task_transition(user_first, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(user_first));
    assert(!nova_address_space_get(user_space_id, &address_space_info));

    assert(nova_task_transition(second, NOVA_TASK_ZOMBIE));
    assert(nova_task_destroy(second));
    assert(freed_stacks == 3);
    puts("Task lifecycle host tests passed.");
    return 0;
}
