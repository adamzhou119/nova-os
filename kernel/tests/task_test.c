#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "task.h"
#include "context.h"
#include "vm.h"

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
static void dummy_entry(void *argument) { (void)argument; }

int main(void) {
    uint32_t first, second;
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
    assert(!nova_task_set_resume_kind(first, NOVA_TASK_RESUME_COOPERATIVE));
    assert(nova_task_mark_irq_resumable(first, info.irq_frame));
    assert(nova_task_get(first, &info));
    assert(info.resume_kind == NOVA_TASK_RESUME_IRQ);
    assert(nova_task_transition(first, NOVA_TASK_READY));
    {
        uint32_t selected = 0U;
        assert(nova_task_next_ready(0U, &selected) && selected == first);
        assert(nova_task_next_cooperative_ready(0U, &selected) && selected == second);
        assert(nova_task_set_resume_kind(first, NOVA_TASK_RESUME_COOPERATIVE));
        assert(nova_task_next_cooperative_ready(0U, &selected) && selected == first);
        assert(!nova_task_destroy(first));
    }
    assert(nova_task_transition(first, NOVA_TASK_RUNNING));
    assert(!nova_task_destroy(first));
    assert(nova_task_set_exit_code(first, -7));
    assert(nova_task_transition(first, NOVA_TASK_BLOCKED));
    assert(!nova_task_set_exit_code(first, 0));
    assert(nova_task_transition(first, NOVA_TASK_READY));
    assert(nova_task_transition(first, NOVA_TASK_RUNNING));
    assert(nova_task_transition(first, NOVA_TASK_ZOMBIE));
    assert(nova_task_get(first, &info) && info.exit_code == -7);
    assert(nova_task_destroy(first));
    assert(!nova_task_get(first, &info));
    assert(nova_task_destroy(second));
    assert(freed_stacks == 2);
    puts("Task lifecycle host tests passed.");
    return 0;
}
