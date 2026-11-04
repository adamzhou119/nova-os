#include "task.h"
#include "ipc.h"
#include "service.h"

#include "context.h"
#include "interrupt.h"
#include "address_space.h"
#include "mmu.h"
#include "user_context.h"

#include <stddef.h>

#include "vm.h"

extern void nova_task_start_trampoline(void);
extern void nova_user_task_start_trampoline(void);

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
    stack = nova_vmap_alloc_guarded_page(NOVA_MMU_MAP_WRITABLE);
    if (stack == 0) return 0;
    id = next_task_id++;
    tasks[i].id = id;
    tasks[i].address_space_id = 0U;
    tasks[i].state = NOVA_TASK_READY;
    tasks[i].resume_kind = NOVA_TASK_RESUME_FIRST_RUN;
    tasks[i].entry = entry;
    tasks[i].argument = argument;
    tasks[i].user_entry = 0U;
    tasks[i].user_stack_top = 0U;
    tasks[i].kernel_stack_base = (uintptr_t)stack;
    tasks[i].kernel_stack_top = (uintptr_t)stack + NOVA_TASK_STACK_SIZE;
    tasks[i].exit_code = 0;
    tasks[i].fault_kind = NOVA_TASK_FAULT_NONE;
    tasks[i].fault_esr = 0U;
    tasks[i].fault_far = 0U;
    tasks[i].fault_elr = 0U;
    tasks[i].irq_frame = (struct nova_irq_frame *)(tasks[i].kernel_stack_top -
                                                    sizeof(struct nova_irq_frame));
    {
        size_t reg;
        for (reg = 0; reg < 31U; ++reg) tasks[i].irq_frame->x[reg] = 0U;
        tasks[i].irq_frame->x[0] = (uintptr_t)argument;
        tasks[i].irq_frame->x[19] = (uintptr_t)entry;
        tasks[i].irq_frame->x[20] = (uintptr_t)argument;
        tasks[i].irq_frame->x[21] = (uintptr_t)&tasks[i];
        tasks[i].irq_frame->x[30] = 0U;
        tasks[i].irq_frame->elr_el1 = (uintptr_t)nova_task_start_trampoline;
        tasks[i].irq_frame->spsr_el1 = 0x5U;
        tasks[i].irq_frame->sp_el0 = 0U;
    }
    if (!nova_arch_context_init(&tasks[i].context,
                                (uintptr_t)tasks[i].irq_frame,
                                (uintptr_t)nova_task_start_trampoline,
                                (uintptr_t)entry, (uintptr_t)argument)) {
        (void)nova_vmap_free_guarded_page(stack);
        tasks[i].id = 0;
        tasks[i].state = NOVA_TASK_UNUSED;
        return 0;
    }
    tasks[i].context.x19_x30[2] = (uintptr_t)&tasks[i];
    *task_id = id;
    return 1;
}

int nova_task_create_user(uint32_t address_space_id, uintptr_t user_entry,
                          uintptr_t user_stack_top, uintptr_t argument0,
                          uint32_t *task_id) {
    size_t i;
    void *stack;
    uint32_t id;
    struct nova_address_space_info space;
    struct nova_irq_frame *frame;

    if (address_space_id == 0U || task_id == 0 || next_task_id == 0U ||
        (user_entry & 3U) != 0U || (user_stack_top & 15U) != 0U ||
        user_entry < NOVA_USER_VA_BASE || user_entry >= NOVA_USER_VA_END ||
        user_stack_top <= NOVA_USER_VA_BASE || user_stack_top > NOVA_USER_VA_END ||
        !nova_address_space_get(address_space_id, &space) ||
        !nova_mmu_user_verify_page(space.root_physical, space.asid,
            user_entry & ~(uintptr_t)4095U, NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_user_verify_page(space.root_physical, space.asid,
            (user_stack_top - 1U) & ~(uintptr_t)4095U,
            NOVA_MMU_MAP_WRITABLE))
        return 0;
    for (i = 0; i < NOVA_TASK_MAX; ++i)
        if (tasks[i].state == NOVA_TASK_UNUSED) break;
    if (i == NOVA_TASK_MAX) return 0;

    stack = nova_vmap_alloc_guarded_page(NOVA_MMU_MAP_WRITABLE);
    if (stack == 0) return 0;
    frame = (struct nova_irq_frame *)((uintptr_t)stack + NOVA_TASK_STACK_SIZE -
                                      sizeof(struct nova_irq_frame));
    if (!nova_user_irq_frame_init(frame, user_entry, user_stack_top, argument0)) {
        (void)nova_vmap_free_guarded_page(stack);
        return 0;
    }
    /*
     * Finish all fallible local setup before retaining the address space.
     * A failed context initialization must not leave an unreachable reference
     * behind if the later final-release hook itself is temporarily failing.
     */
    if (!nova_arch_context_init(&tasks[i].context,
                                (uintptr_t)stack + NOVA_TASK_STACK_SIZE,
                                (uintptr_t)nova_user_task_start_trampoline,
                                (uintptr_t)frame, 0U)) {
        (void)nova_vmap_free_guarded_page(stack);
        return 0;
    }
    if (!nova_address_space_retain(address_space_id)) {
        (void)nova_vmap_free_guarded_page(stack);
        tasks[i].context.sp = 0U;
        return 0;
    }

    id = next_task_id++;
    tasks[i].id = id;
    tasks[i].address_space_id = address_space_id;
    tasks[i].state = NOVA_TASK_READY;
    tasks[i].resume_kind = NOVA_TASK_RESUME_USER_FIRST_RUN;
    tasks[i].entry = 0;
    tasks[i].argument = (void *)argument0;
    tasks[i].kernel_stack_base = (uintptr_t)stack;
    tasks[i].kernel_stack_top = (uintptr_t)stack + NOVA_TASK_STACK_SIZE;
    tasks[i].exit_code = 0;
    tasks[i].fault_kind = NOVA_TASK_FAULT_NONE;
    tasks[i].fault_esr = 0U;
    tasks[i].fault_far = 0U;
    tasks[i].fault_elr = 0U;
    tasks[i].irq_frame = frame;
    tasks[i].user_entry = user_entry;
    tasks[i].user_stack_top = user_stack_top;
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
    size_t i;
    if (task == 0 || info == 0) return 0;
    info->id = task->id;
    info->address_space_id = task->address_space_id;
    info->state = task->state;
    info->resume_kind = task->resume_kind;
    info->entry = task->entry;
    info->argument = task->argument;
    info->kernel_stack_base = task->kernel_stack_base;
    info->kernel_stack_top = task->kernel_stack_top;
    info->exit_code = task->exit_code;
    info->fault_kind = task->fault_kind;
    info->fault_esr = task->fault_esr;
    info->fault_far = task->fault_far;
    info->fault_elr = task->fault_elr;
    for (i = 0; i < 12U; ++i)
        info->context.x19_x30[i] = task->context.x19_x30[i];
    info->context.sp = task->context.sp;
    info->irq_frame = task->irq_frame;
    info->user_entry = task->user_entry;
    info->user_stack_top = task->user_stack_top;
    return 1;
}

int nova_task_set_address_space(uint32_t task_id, uint32_t address_space_id) {
    struct nova_task_info *task = find_task(task_id);
    uint32_t old_id;
    if (task == 0 || task->state != NOVA_TASK_READY ||
        task->resume_kind != NOVA_TASK_RESUME_FIRST_RUN)
        return 0;
    old_id = task->address_space_id;
    if (old_id == address_space_id) return 1;
    if (address_space_id != 0U &&
        !nova_address_space_retain(address_space_id))
        return 0;
    if (old_id != 0U && !nova_address_space_release(old_id)) {
        if (address_space_id != 0U)
            (void)nova_address_space_release(address_space_id);
        return 0;
    }
    task->address_space_id = address_space_id;
    return 1;
}


int nova_task_set_irq_frame(uint32_t task_id, struct nova_irq_frame *frame) {
    struct nova_task_info *task = find_task(task_id);
    uintptr_t address = (uintptr_t)frame;
    if (task == 0 || frame == 0 ||
        (address & 15U) != 0U ||
        address < task->kernel_stack_base ||
        address > task->kernel_stack_top - sizeof(*frame))
        return 0;
    task->irq_frame = frame;
    return 1;
}

int nova_task_mark_irq_resumable(uint32_t task_id, struct nova_irq_frame *frame) {
    struct nova_task_info *task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_RUNNING ||
        !nova_task_set_irq_frame(task_id, frame))
        return 0;
    task->resume_kind = NOVA_TASK_RESUME_IRQ;
    return 1;
}

int nova_task_save_irq_context(uint32_t task_id, struct nova_irq_frame *frame) {
    struct nova_task_info *task = find_task(task_id);
    uintptr_t address = (uintptr_t)frame;

    /*
     * Publish the exception frame and READY state as one task-table update.
     * The IRQ caller must already have local IRQ delivery masked.
     */
    if (task == 0 || task->state != NOVA_TASK_RUNNING || frame == 0 ||
        (address & 15U) != 0U ||
        address < task->kernel_stack_base ||
        address > task->kernel_stack_top - sizeof(*frame))
        return 0;
    task->irq_frame = frame;
    task->resume_kind = NOVA_TASK_RESUME_IRQ;
    task->state = NOVA_TASK_READY;
    return 1;
}

int nova_task_take_irq_context(uint32_t task_id,
                                 struct nova_irq_frame **frame) {
    struct nova_task_info *task = find_task(task_id);
    struct nova_irq_frame *saved_frame;
    uintptr_t address;

    if (task == 0 || frame == 0 || task->state != NOVA_TASK_READY ||
        (task->resume_kind != NOVA_TASK_RESUME_IRQ &&
         task->resume_kind != NOVA_TASK_RESUME_USER_FIRST_RUN) ||
        task->irq_frame == 0)
        return 0;

    saved_frame = task->irq_frame;
    address = (uintptr_t)saved_frame;
    if ((address & 15U) != 0U ||
        address < task->kernel_stack_base ||
        address > task->kernel_stack_top - sizeof(*saved_frame))
        return 0;
    task->state = NOVA_TASK_RUNNING;
    task->resume_kind = NOVA_TASK_RESUME_ACTIVE;
    task->irq_frame = 0;
    *frame = saved_frame;
    return 1;
}

int nova_task_irq_handoff(uint32_t current_id, struct nova_irq_frame *frame,
                           uint32_t *next_id, struct nova_irq_frame **next_frame) {
    struct nova_task_info *current = find_task(current_id);
    struct nova_task_info *selected;
    struct nova_irq_frame *selected_irq_frame;
    uint32_t selected_id;

    if (current == 0 || current->state != NOVA_TASK_RUNNING ||
        next_id == 0 || next_frame == 0 || frame == 0)
        return 0;
    if (!nova_task_save_irq_context(current_id, frame)) return 0;

    if (!nova_task_next_irq_ready(current_id, &selected_id) ||
        selected_id == current_id) {
        /* ERET resumes this task immediately; this frame is not resumable later. */
        current->state = NOVA_TASK_RUNNING;
        current->irq_frame = 0;
        current->resume_kind = NOVA_TASK_RESUME_ACTIVE;
        *next_id = current_id;
        *next_frame = frame;
        return 0;
    }

    selected = find_task(selected_id);
    if (selected == 0 || selected->state != NOVA_TASK_READY ||
        (selected->resume_kind != NOVA_TASK_RESUME_FIRST_RUN &&
         selected->resume_kind != NOVA_TASK_RESUME_USER_FIRST_RUN &&
         selected->resume_kind != NOVA_TASK_RESUME_IRQ) ||
        selected->irq_frame == 0) {
        current->state = NOVA_TASK_RUNNING;
        current->irq_frame = 0;
        current->resume_kind = NOVA_TASK_RESUME_ACTIVE;
        *next_id = current_id;
        *next_frame = frame;
        return 0;
    }

    selected_irq_frame = selected->irq_frame;
    selected->state = NOVA_TASK_RUNNING;
    selected->irq_frame = 0;
    selected->resume_kind = NOVA_TASK_RESUME_ACTIVE;
    *next_id = selected_id;
    *next_frame = selected_irq_frame;
    return 1;
}

int nova_task_set_resume_kind(uint32_t task_id, enum nova_task_resume_kind kind) {
    struct nova_task_info *task = find_task(task_id);
    if (task == 0 ||
        (kind != NOVA_TASK_RESUME_COOPERATIVE &&
         kind != NOVA_TASK_RESUME_IRQ &&
         kind != NOVA_TASK_RESUME_ACTIVE))
        return 0;
    /*
     * FIRST_RUN is creation-only. A cooperative context becomes resumable
     * only after the task has yielded/blocked; an IRQ frame is published
     * only while the interrupted task is still RUNNING.
     */
    if (kind == NOVA_TASK_RESUME_COOPERATIVE &&
        task->state != NOVA_TASK_READY && task->state != NOVA_TASK_BLOCKED)
        return 0;
    if ((kind == NOVA_TASK_RESUME_IRQ ||
         kind == NOVA_TASK_RESUME_ACTIVE) &&
        task->state != NOVA_TASK_RUNNING)
        return 0;
    task->resume_kind = kind;
    return 1;
}

static int next_ready_matching(uint32_t after_id, uint32_t *task_id,
                               int resume_path) {
    struct nova_task_info *wrapped = 0;
    struct nova_task_info *next = 0;
    size_t i;
    if (task_id == 0) return 0;
    for (i = 0; i < NOVA_TASK_MAX; ++i) {
        if (tasks[i].state != NOVA_TASK_READY ||
            (resume_path == 1 &&
             (tasks[i].resume_kind == NOVA_TASK_RESUME_IRQ ||
              tasks[i].resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN)) ||
            (resume_path == 2 &&
             tasks[i].resume_kind == NOVA_TASK_RESUME_COOPERATIVE))
            continue;
        if (wrapped == 0 || tasks[i].id < wrapped->id) wrapped = &tasks[i];
        if (tasks[i].id > after_id &&
            (next == 0 || tasks[i].id < next->id)) next = &tasks[i];
    }
    if (next == 0) next = wrapped;
    if (next == 0) return 0;
    *task_id = next->id;
    return 1;
}

int nova_task_next_ready(uint32_t after_id, uint32_t *task_id) {
    return next_ready_matching(after_id, task_id, 0);
}

int nova_task_next_cooperative_ready(uint32_t after_id, uint32_t *task_id) {
    return next_ready_matching(after_id, task_id, 1);
}

int nova_task_next_irq_ready(uint32_t after_id, uint32_t *task_id) {
    return next_ready_matching(after_id, task_id, 2);
}

struct nova_cpu_context *nova_task_context(uint32_t task_id) {
    struct nova_task_info *task = find_task(task_id);
    return task == 0 ? 0 : &task->context;
}

void nova_task_mark_zombie(struct nova_task_info *task) {
    if (task != 0 && task->state == NOVA_TASK_RUNNING)
        task->state = NOVA_TASK_ZOMBIE;
}

int nova_task_set_exit_code(uint32_t task_id, int exit_code) {
    struct nova_task_info *task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_RUNNING) return 0;
    task->exit_code = exit_code;
    return 1;
}

int nova_task_set_fault(uint32_t task_id, enum nova_task_fault_kind kind,
                        uint64_t esr, uint64_t far, uint64_t elr) {
    struct nova_task_info *task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_RUNNING ||
        (kind <= NOVA_TASK_FAULT_NONE || kind > NOVA_TASK_FAULT_BRK))
        return 0;
    task->fault_kind = kind;
    task->fault_esr = esr;
    task->fault_far = far;
    task->fault_elr = elr;
    task->exit_code = NOVA_TASK_EXIT_FAULT;
    return 1;
}

int nova_task_reap_zombies(void) {
    uint32_t ids[NOVA_TASK_MAX];
    size_t i, count = 0U, reaped = 0U;

    /* Snapshot IDs first: successful destruction clears and reuses TCB slots. */
    for (i = 0; i < NOVA_TASK_MAX; ++i) {
        if (tasks[i].state == NOVA_TASK_ZOMBIE)
            ids[count++] = tasks[i].id;
    }
    for (i = 0; i < count; ++i)
        if (nova_task_destroy(ids[i])) ++reaped;
    return (int)reaped;
}

int nova_task_destroy(uint32_t task_id) {
    struct nova_task_info *task = find_task(task_id);
    uintptr_t stack;
    if (task == 0 ||
        (task->state != NOVA_TASK_ZOMBIE &&
         !(task->state == NOVA_TASK_READY &&
           (task->resume_kind == NOVA_TASK_RESUME_FIRST_RUN ||
            task->resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN))))
        return 0;
    stack = task->kernel_stack_base;
    /*
     * Free the non-runnable task's kernel stack before dropping its final
     * address-space reference: the release hook may irreversibly destroy the
     * page-table root. If teardown then fails, keep a stackless ZOMBIE record
     * so a later destroy call can retry without resurrecting freed resources.
     */
    if (stack != 0U && !nova_vmap_free_guarded_page((void *)stack))
        return 0;
    task->kernel_stack_base = 0U;
    task->kernel_stack_top = 0U;
    task->irq_frame = 0;
    task->state = NOVA_TASK_ZOMBIE;
    /* Revoke owned IPC endpoints once the task can no longer run. */
    nova_service_close_owner(task_id);
    nova_ipc_close_owner(task_id);
    if (task->address_space_id != 0U &&
        !nova_address_space_release(task->address_space_id))
        return 0;
    task->id = 0;
    task->address_space_id = 0U;
    task->state = NOVA_TASK_UNUSED;
    task->resume_kind = NOVA_TASK_RESUME_FIRST_RUN;
    task->entry = 0;
    task->argument = 0;
    task->user_entry = 0U;
    task->user_stack_top = 0U;
    task->kernel_stack_base = 0;
    task->kernel_stack_top = 0;
    task->exit_code = 0;
    task->fault_kind = NOVA_TASK_FAULT_NONE;
    task->fault_esr = 0U;
    task->fault_far = 0U;
    task->fault_elr = 0U;
    {
        size_t i;
        for (i = 0; i < 12U; ++i) task->context.x19_x30[i] = 0;
        task->context.sp = 0;
        task->irq_frame = 0;
    }
    return 1;
}
