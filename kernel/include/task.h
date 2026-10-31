#ifndef NOVA_TASK_H
#define NOVA_TASK_H

#include <stdint.h>
#include "context.h"
#include "interrupt.h"

#define NOVA_TASK_MAX 16U

typedef void (*nova_task_entry_t)(void *argument);

enum nova_task_state {
    NOVA_TASK_UNUSED = 0,
    NOVA_TASK_READY,
    NOVA_TASK_RUNNING,
    NOVA_TASK_BLOCKED,
    NOVA_TASK_ZOMBIE
};

struct nova_task_info {
    uint32_t id;
    enum nova_task_state state;
    nova_task_entry_t entry;
    void *argument;
    uintptr_t kernel_stack_base;
    uintptr_t kernel_stack_top;
    int exit_code;
    struct nova_cpu_context context;
    struct nova_irq_frame *irq_frame;
};

/* Creates a task control block and one mapped kernel stack page. */
int nova_task_create(nova_task_entry_t entry, void *argument, uint32_t *task_id);
/* State changes are validated; this does not yet perform a CPU context switch. */
int nova_task_transition(uint32_t task_id, enum nova_task_state next_state);
int nova_task_get(uint32_t task_id, struct nova_task_info *info);
/* Records the stack-resident exception frame for a task after IRQ entry. */
int nova_task_set_irq_frame(uint32_t task_id, struct nova_irq_frame *frame);
struct nova_cpu_context *nova_task_context(uint32_t task_id);
int nova_task_next_ready(uint32_t after_id, uint32_t *task_id);
void nova_task_mark_zombie(struct nova_task_info *task);
int nova_task_set_exit_code(uint32_t task_id, int exit_code);
/* Only READY (never dispatched) and ZOMBIE tasks may be destroyed. */
int nova_task_destroy(uint32_t task_id);

#endif
