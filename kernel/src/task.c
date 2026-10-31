#include "task.h"

#include "vm.h"

#define NOVA_TASK_STACK_SIZE 4096U

static struct nova_task_info tasks[NOVA_TASK_MAX];
static uint32_t next_task_id = 1U;

static struct nova_task_info *find_task(uint32_t id) {
    size_t i;
    if (id == 0U) return 0;
    for (i = 0; i < NOVA_TASK_MAX; ++i)
        if (tasks[i].state != NOVA_TASK_UNUSED && tasks[i].id == id)
            return &tasks[i];
    return 0;
}

int nova_task_create(nova_task_entry_t entry, void *argument, uint32_t *task_id) {
    size_t i;
    void *stack;
    uint32_t id;
    if (entry == 0 || task_id == 0 || next_task_id == 0U) return 0;
    for (i = 0; i < NOVA_TASK_MAX; ++i)
        if (tasks[i].state == NOVA_TASK_UNUSED) break;
    if (i == NOVA_TASK_MAX) return 0;
    stack = nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE);
    if (stack == 0) return 0;
    id = next_task_id++;
    tasks[i].id = id;
    tasks[i].state = NOVA_TASK_READY;
    tasks[i].entry = entry;
    tasks[i].argument = argument;
    tasks[i].kernel_stack_base = (uintptr_t)stack;
    tasks[i].kernel_stack_top = (uintptr_t)stack + NOVA_TASK_STACK_SIZE;
    tasks[i].exit_code = 0;
    *task_id = id;
    return 1;
}

int nova_task_transition(uint32_t task_id, enum nova_task_state next_state) {
    struct nova_task_info *task = find_task(task_id);
    int allowed = 0;
    if (task == 0) return 0;
    switch (task->state) {
    case NOVA_TASK_READY:
        allowed = next_state == NOVA_TASK_RUNNING;
        break;
    case NOVA_TASK_RUNNING:
        allowed = next_state == NOVA_TASK_READY ||
                  next_state == NOVA_TASK_BLOCKED ||
                  next_state == NOVA_TASK_ZOMBIE;
        break;
    case NOVA_TASK_BLOCKED:
        allowed = next_state == NOVA_TASK_READY;
        break;
    default:
        break;
    }
    if (!allowed) return 0;
    task->state = next_state;
    return 1;
}

int nova_task_get(uint32_t task_id, struct nova_task_info *info) {
    struct nova_task_info *task = find_task(task_id);
    if (task == 0 || info == 0) return 0;
    *info = *task;
    return 1;
}

int nova_task_set_exit_code(uint32_t task_id, int exit_code) {
    struct nova_task_info *task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_RUNNING) return 0;
    task->exit_code = exit_code;
    return 1;
}

int nova_task_destroy(uint32_t task_id) {
    struct nova_task_info *task = find_task(task_id);
    uintptr_t stack;
    if (task == 0 || (task->state != NOVA_TASK_READY &&
                      task->state != NOVA_TASK_ZOMBIE)) return 0;
    stack = task->kernel_stack_base;
    if (!nova_vmap_free_page((void *)stack)) return 0;
    task->id = 0;
    task->state = NOVA_TASK_UNUSED;
    task->entry = 0;
    task->argument = 0;
    task->kernel_stack_base = 0;
    task->kernel_stack_top = 0;
    task->exit_code = 0;
    return 1;
}
