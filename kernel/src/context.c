#include "context.h"
#include "user_context.h"
#include "interrupt.h"
#include "mmu.h"

/*
 * Host-side lifecycle tests do not link the AArch64 trampoline assembly.
 * The real kernel's strong assembly definition overrides this weak stub.
 */
__attribute__((weak)) void nova_user_task_start_trampoline(void) {
}

int nova_arch_context_init(struct nova_cpu_context *context,
                           uintptr_t stack_top, uintptr_t entry,
                           uintptr_t argument0, uintptr_t argument1) {
    unsigned int i;
    if (context == 0 || stack_top == 0 || entry == 0 ||
        (stack_top & 15U) != 0U) return 0;
    for (i = 0; i < 12U; ++i) context->x19_x30[i] = 0;
    context->x19_x30[0] = argument0;
    context->x19_x30[1] = argument1;
    context->x19_x30[11] = entry;
    context->sp = stack_top;
    return 1;
}

/*
 * Prepare the common IRQ-return frame for a first EL0 dispatch. The caller
 * remains responsible for mapping entry and stack with EL0 permissions in
 * the address space that will be active before nova_arch_irq_resume().
 */
int nova_user_irq_frame_init(struct nova_irq_frame *frame,
                             uintptr_t user_entry, uintptr_t user_stack_top,
                             uintptr_t argument0) {
    unsigned int i;
    if (frame == 0 || user_entry < NOVA_USER_VA_BASE ||
        user_entry >= NOVA_USER_VA_END || (user_entry & 3U) != 0U ||
        user_stack_top <= NOVA_USER_VA_BASE ||
        user_stack_top > NOVA_USER_VA_END || (user_stack_top & 15U) != 0U)
        return 0;
    for (i = 0; i < 31U; ++i) frame->x[i] = 0U;
    frame->x[0] = argument0;
    frame->elr_el1 = user_entry;
    frame->spsr_el1 = 0U; /* AArch64 EL0t; normal interrupt mask state. */
    frame->sp_el0 = user_stack_top;
    return 1;
}
