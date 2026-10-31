#include "context.h"

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
