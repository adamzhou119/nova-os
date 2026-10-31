#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "task.h"
#include "vm.h"

static uintptr_t next_stack = 0x48000000U;
static int freed_stacks;

void *nova_vmap_alloc_page(unsigned int flags) {
    uintptr_t result = next_stack;
    (void)flags;
    next_stack += 4096U;
    return (void *)result;
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
    int argument = 42;
    assert(!nova_task_create(0, &argument, &first));
    assert(!nova_task_create(dummy_entry, &argument, 0));
    assert(nova_task_create(dummy_entry, &argument, &first));
    assert(nova_task_create(dummy_entry, 0, &second));
    assert(first != second);
    assert(nova_task_get(first, &info));
    assert(info.state == NOVA_TASK_READY);
    assert(info.entry == dummy_entry && info.argument == &argument);
    assert(info.kernel_stack_top - info.kernel_stack_base == 4096U);
    assert((info.kernel_stack_top & 15U) == 0U);
    assert(!nova_task_transition(first, NOVA_TASK_BLOCKED));
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
