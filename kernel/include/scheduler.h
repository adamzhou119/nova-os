#ifndef NOVA_SCHEDULER_H
#define NOVA_SCHEDULER_H

#include <stdint.h>

/* Single-core cooperative round-robin scheduler. No timer preemption yet. */
int nova_sched_start(void);
int nova_sched_yield(void);
int nova_sched_block_current(void);
int nova_sched_unblock(uint32_t task_id);
uint32_t nova_sched_current(void);
/* Called only by the first-run trampoline after marking the task ZOMBIE. */
void nova_sched_task_exit(void);

#endif
