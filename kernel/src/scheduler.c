#include "scheduler.h"

#include "context.h"
#include "task.h"

#define NOVA_SCHED_MAX_TASKS NOVA_TASK_MAX

static struct nova_cpu_context scheduler_context;
static uint32_t current_task_id;
static size_t next_slot;
static int scheduler_active;

static int find_next_ready(uint32_t *task_id) {
    size_t n;
    for (n = 0; n < NOVA_SCHED_MAX_TASKS; ++n) {
        size_t slot = (next_slot + n) % NOVA_SCHED_MAX_TASKS;
        struct nova_task_info info;
        /* Task IDs are monotonic; the table is intentionally bounded. */
        uint32_t candidate;
        static uint32_t probe_id = 1U;
        (void)probe_id;
        candidate = (uint32_t)slot + 1U;
        (void)candidate;
        /*
         * The task API exposes IDs, not table slots. Scan the monotonically
         * assigned ID space and use task_get to skip destroyed entries.
         */
        {
            uint32_t id;
            for (id = 1U; id != 0U; ++id) {
                if (nova_task_get(id, &info) && info.state == NOVA_TASK_READY) {
                    *task_id = id;
                    next_slot = (slot + 1U) % NOVA_SCHED_MAX_TASKS;
                    return 1;
                }
                if (id > 65535U) break;
            }
        }
    }
    return 0;
}

static int switch_to_scheduler(void) {
    struct nova_cpu_context *task_context;
    if (!scheduler_active || current_task_id == 0U) return 0;
    task_context = nova_task_context(current_task_id);
    if (task_context == 0) return 0;
    nova_arch_context_switch(task_context, &scheduler_context);
    return 1;
}

int nova_sched_start(void) {
    uint32_t next;
    if (scheduler_active) return 0;
    scheduler_active = 1;
    current_task_id = 0U;
    while (find_next_ready(&next)) {
        struct nova_cpu_context *context = nova_task_context(next);
        if (context == 0 || !nova_task_transition(next, NOVA_TASK_RUNNING))
            continue;
        current_task_id = next;
        nova_arch_context_switch(&scheduler_context, context);
        current_task_id = 0U;
    }
    scheduler_active = 0;
    return 1;
}

int nova_sched_yield(void) {
    uint32_t id = current_task_id;
    if (id == 0U || !nova_task_transition(id, NOVA_TASK_READY)) return 0;
    return switch_to_scheduler();
}

int nova_sched_block_current(void) {
    uint32_t id = current_task_id;
    if (id == 0U || !nova_task_transition(id, NOVA_TASK_BLOCKED)) return 0;
    return switch_to_scheduler();
}

int nova_sched_unblock(uint32_t task_id) {
    return nova_task_transition(task_id, NOVA_TASK_READY);
}

uint32_t nova_sched_current(void) {
    return current_task_id;
}

void nova_sched_task_exit(void) {
    if (current_task_id == 0U) for (;;) __asm__ volatile("wfe");
    (void)switch_to_scheduler();
    for (;;) __asm__ volatile("wfe");
}
