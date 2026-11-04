#include "task.h"
#include "ipc.h"
#include "service.h"
#include "capability.h"

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

static uintptr_t task_irq_save_disable(void) {
#if defined(__aarch64__)
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2"
                     : "=r"(daif) :: "memory");
    return daif;
#else
    return 0U;
#endif
}

static void task_irq_restore(uintptr_t daif) {
#if defined(__aarch64__)
    __asm__ volatile("msr daif, %0" :: "r"(daif) : "memory");
#else
    (void)daif;
#endif
}

static int task_frame_in_stack(const struct nova_task_info *task,
                               const struct nova_irq_frame *frame) {
    uintptr_t address = (uintptr_t)frame;

    if (task == 0 || frame == 0 || task->kernel_stack_base == 0U ||
        task->kernel_stack_top < task->kernel_stack_base ||
        task->kernel_stack_top - task->kernel_stack_base < sizeof(*frame) ||
        (address & 15U) != 0U)
        return 0;
    return address >= task->kernel_stack_base &&
           address <= task->kernel_stack_top - sizeof(*frame);
}

static struct nova_task_info *find_task(uint32_t id) {
    size_t i;
    if (id == 0U) return 0;
    for (i = 0; i < NOVA_TASK_MAX; ++i)
        if (tasks[i].state != NOVA_TASK_UNUSED &&
            tasks[i].state != NOVA_TASK_CREATING && tasks[i].id == id)
            return &tasks[i];
    return 0;
}

/*
 * Reserve both the TCB slot and its never-reused identity before doing any
 * fallible allocation. IRQ masking makes the reservation atomic against a
 * second creator interrupted on this single-core kernel.
 */
static int task_reserve_slot(size_t *slot, uint32_t *id) {
    uintptr_t saved_daif;
    size_t i;

    if (slot == 0 || id == 0) return 0;
    saved_daif = task_irq_save_disable();
    if (next_task_id == 0U) {
        task_irq_restore(saved_daif);
        return 0;
    }
    for (i = 0; i < NOVA_TASK_MAX; ++i)
        if (tasks[i].state == NOVA_TASK_UNUSED) break;
    if (i == NOVA_TASK_MAX) {
        task_irq_restore(saved_daif);
        return 0;
    }
    *id = next_task_id++;
    tasks[i].id = *id;
    tasks[i].state = NOVA_TASK_CREATING;
    task_irq_restore(saved_daif);
    *slot = i;
    return 1;
}

static void task_cancel_reservation(size_t slot, uint32_t id) {
    uintptr_t saved_daif = task_irq_save_disable();
    if (slot < NOVA_TASK_MAX &&
        tasks[slot].state == NOVA_TASK_CREATING && tasks[slot].id == id) {
        tasks[slot].id = 0U;
        tasks[slot].state = NOVA_TASK_UNUSED;
    }
    task_irq_restore(saved_daif);
}

static int task_publish_ready(size_t slot, uint32_t id) {
    uintptr_t saved_daif = task_irq_save_disable();
    int published = 0;
    if (slot < NOVA_TASK_MAX &&
        tasks[slot].state == NOVA_TASK_CREATING && tasks[slot].id == id) {
        __asm__ volatile("" ::: "memory");
        tasks[slot].state = NOVA_TASK_READY;
        published = 1;
    }
    task_irq_restore(saved_daif);
    return published;
}

int nova_task_create(nova_task_entry_t entry, void *argument, uint32_t *task_id) {
    size_t i;
    void *stack;
    uint32_t id;
    if (entry == 0 || task_id == 0 || !task_reserve_slot(&i, &id)) return 0;
    stack = nova_vmap_alloc_guarded_page(NOVA_MMU_MAP_WRITABLE);
    if (stack == 0) {
        task_cancel_reservation(i, id);
        return 0;
    }
    tasks[i].id = id;
    tasks[i].address_space_id = 0U;
    /*
     * Keep the TCB unpublished while its stack frame and cooperative context
     * are being initialized. A timer IRQ may enter the scheduler at any point
     * in task creation; READY is the publication point and must be last.
     */
    tasks[i].resume_kind = NOVA_TASK_RESUME_FIRST_RUN;
    tasks[i].entry = entry;
    tasks[i].argument = argument;
    tasks[i].user_entry = 0U;
    tasks[i].user_stack_top = 0U;
    tasks[i].teardown_in_progress = 0U;
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
        task_cancel_reservation(i, id);
        return 0;
    }
    tasks[i].context.x19_x30[2] = (uintptr_t)&tasks[i];
    /* Publish only after every field consumed by the scheduler is valid. */
    if (!task_publish_ready(i, id)) {
        (void)nova_vmap_free_guarded_page(stack);
        task_cancel_reservation(i, id);
        return 0;
    }
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

    if (address_space_id == 0U || task_id == 0 ||
        (user_entry & 3U) != 0U || (user_stack_top & 15U) != 0U ||
        user_entry < NOVA_USER_VA_BASE || user_entry >= NOVA_USER_VA_END ||
        user_stack_top <= NOVA_USER_VA_BASE || user_stack_top > NOVA_USER_VA_END)
        return 0;

    /*
     * Acquire ownership before reading the registry snapshot. This prevents a
     * concurrent final release from destroying the page tables between get()
     * and retain(). The reference remains owned until READY publication, or
     * is released on every failure path below.
     */
    if (!nova_address_space_retain(address_space_id))
        return 0;
    if (!nova_address_space_get(address_space_id, &space) ||
        !nova_mmu_user_verify_page(space.root_physical, space.asid,
            user_entry & ~(uintptr_t)4095U, NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_user_verify_page(space.root_physical, space.asid,
            (user_stack_top - 1U) & ~(uintptr_t)4095U,
            NOVA_MMU_MAP_WRITABLE)) {
        (void)nova_address_space_release(address_space_id);
        return 0;
    }
    if (!task_reserve_slot(&i, &id)) {
        (void)nova_address_space_release(address_space_id);
        return 0;
    }

    stack = nova_vmap_alloc_guarded_page(NOVA_MMU_MAP_WRITABLE);
    if (stack == 0) {
        task_cancel_reservation(i, id);
        (void)nova_address_space_release(address_space_id);
        return 0;
    }
    frame = (struct nova_irq_frame *)((uintptr_t)stack + NOVA_TASK_STACK_SIZE -
                                      sizeof(struct nova_irq_frame));
    if (!nova_user_irq_frame_init(frame, user_entry, user_stack_top, argument0)) {
        (void)nova_vmap_free_guarded_page(stack);
        task_cancel_reservation(i, id);
        (void)nova_address_space_release(address_space_id);
        return 0;
    }
    if (!nova_arch_context_init(&tasks[i].context,
                                (uintptr_t)stack + NOVA_TASK_STACK_SIZE,
                                (uintptr_t)nova_user_task_start_trampoline,
                                (uintptr_t)frame, 0U)) {
        (void)nova_vmap_free_guarded_page(stack);
        tasks[i].context.sp = 0U;
        task_cancel_reservation(i, id);
        (void)nova_address_space_release(address_space_id);
        return 0;
    }

    tasks[i].id = id;
    tasks[i].address_space_id = address_space_id;
    /* Do not expose the TCB until its EL0 frame and kernel stack are complete. */
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
    tasks[i].teardown_in_progress = 0U;
    /* READY is the publication point for both kernel and EL0 tasks. */
    if (!task_publish_ready(i, id)) {
        (void)nova_address_space_release(address_space_id);
        (void)nova_vmap_free_guarded_page(stack);
        task_cancel_reservation(i, id);
        return 0;
    }
    *task_id = id;
    return 1;
}

int nova_task_transition(uint32_t task_id, enum nova_task_state next_state) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();
    int allowed = 0;

    task = find_task(task_id);
    if (task == 0) {
        task_irq_restore(saved_daif);
        return 0;
    }
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
    if (!allowed) {
        task_irq_restore(saved_daif);
        return 0;
    }
    task->state = next_state;
    task_irq_restore(saved_daif);
    return 1;
}

int nova_task_block_ready(uint32_t task_id) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();

    task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_READY ||
        (task->resume_kind != NOVA_TASK_RESUME_FIRST_RUN &&
         task->resume_kind != NOVA_TASK_RESUME_USER_FIRST_RUN)) {
        task_irq_restore(saved_daif);
        return 0;
    }
    task->state = NOVA_TASK_BLOCKED;
    task_irq_restore(saved_daif);
    return 1;
}

int nova_task_get(uint32_t task_id, struct nova_task_info *info) {
    struct nova_task_info *task;
    uintptr_t saved_daif;
    size_t i;

    if (info == 0) return 0;
    /*
     * Return one coherent TCB snapshot. On the single-core AArch64 path,
     * local IRQ masking prevents a timer handoff or teardown from changing
     * the task between the individual field reads.
     */
    saved_daif = task_irq_save_disable();
    task = find_task(task_id);
    if (task == 0) {
        task_irq_restore(saved_daif);
        return 0;
    }
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
    info->teardown_in_progress = task->teardown_in_progress;
    task_irq_restore(saved_daif);
    return 1;
}

int nova_task_set_address_space(uint32_t task_id, uint32_t address_space_id) {
    struct nova_task_info *task;
    uint32_t old_id;
    uintptr_t saved_daif = task_irq_save_disable();

    /*
     * A READY first-run task is scheduler-visible. Keep IRQ dispatch out of
     * the retain/release/field-update sequence so the scheduler cannot enter
     * it with an old address-space ID while its references are being changed.
     */
    task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_READY ||
        task->resume_kind != NOVA_TASK_RESUME_FIRST_RUN) {
        task_irq_restore(saved_daif);
        return 0;
    }
    old_id = task->address_space_id;
    if (old_id == address_space_id) {
        task_irq_restore(saved_daif);
        return 1;
    }
    if (address_space_id != 0U &&
        !nova_address_space_retain(address_space_id)) {
        task_irq_restore(saved_daif);
        return 0;
    }
    if (old_id != 0U && !nova_address_space_release(old_id)) {
        if (address_space_id != 0U)
            (void)nova_address_space_release(address_space_id);
        task_irq_restore(saved_daif);
        return 0;
    }
    task->address_space_id = address_space_id;
    task_irq_restore(saved_daif);
    return 1;
}


int nova_task_set_irq_frame(uint32_t task_id, struct nova_irq_frame *frame) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();
    int result = 0;

    task = find_task(task_id);
    if (task != 0 &&
        (task->state == NOVA_TASK_READY ||
         task->state == NOVA_TASK_RUNNING) &&
        task_frame_in_stack(task, frame)) {
        task->irq_frame = frame;
        result = 1;
    }
    task_irq_restore(saved_daif);
    return result;
}

int nova_task_mark_irq_resumable(uint32_t task_id, struct nova_irq_frame *frame) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();
    int result = 0;

    task = find_task(task_id);
    if (task != 0 && task->state == NOVA_TASK_RUNNING &&
        nova_task_set_irq_frame(task_id, frame)) {
        task->resume_kind = NOVA_TASK_RESUME_IRQ;
        result = 1;
    }
    task_irq_restore(saved_daif);
    return result;
}

int nova_task_save_irq_context(uint32_t task_id, struct nova_irq_frame *frame) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();
    int result = 0;

    /* Publish the exception frame and READY state as one task-table update. */
    task = find_task(task_id);
    if (task != 0 && task->state == NOVA_TASK_RUNNING &&
        task_frame_in_stack(task, frame)) {
        task->irq_frame = frame;
        task->resume_kind = NOVA_TASK_RESUME_IRQ;
        task->state = NOVA_TASK_READY;
        result = 1;
    }
    task_irq_restore(saved_daif);
    return result;
}

int nova_task_take_irq_context(uint32_t task_id,
                                 struct nova_irq_frame **frame) {
    struct nova_task_info *task;
    struct nova_irq_frame *saved_frame;
    uintptr_t saved_daif = task_irq_save_disable();
    int result = 0;

    task = find_task(task_id);
    if (task != 0 && frame != 0 &&
        task->state == NOVA_TASK_READY &&
        (task->resume_kind == NOVA_TASK_RESUME_IRQ ||
         task->resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN) &&
        task->irq_frame != 0) {
        saved_frame = task->irq_frame;
        if (task_frame_in_stack(task, saved_frame)) {
            task->state = NOVA_TASK_RUNNING;
            task->resume_kind = NOVA_TASK_RESUME_ACTIVE;
            task->irq_frame = 0;
            *frame = saved_frame;
            result = 1;
        }
    }
    task_irq_restore(saved_daif);
    return result;
}

int nova_task_irq_handoff(uint32_t current_id, struct nova_irq_frame *frame,
                           uint32_t *next_id, struct nova_irq_frame **next_frame) {
    struct nova_task_info *current;
    struct nova_task_info *selected;
    struct nova_irq_frame *selected_irq_frame;
    uint32_t selected_id;
    uintptr_t saved_daif = task_irq_save_disable();

    current = find_task(current_id);
    if (current == 0 || current->state != NOVA_TASK_RUNNING ||
        next_id == 0 || next_frame == 0 || frame == 0) {
        task_irq_restore(saved_daif);
        return 0;
    }
    if (!nova_task_save_irq_context(current_id, frame)) {
        task_irq_restore(saved_daif);
        return 0;
    }

    if (!nova_task_next_irq_ready(current_id, &selected_id) ||
        selected_id == current_id) {
        /* ERET resumes this task immediately; this frame is not resumable later. */
        current->state = NOVA_TASK_RUNNING;
        current->irq_frame = 0;
        current->resume_kind = NOVA_TASK_RESUME_ACTIVE;
        *next_id = current_id;
        *next_frame = frame;
        task_irq_restore(saved_daif);
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
        task_irq_restore(saved_daif);
        return 0;
    }

    selected_irq_frame = selected->irq_frame;
    selected->state = NOVA_TASK_RUNNING;
    selected->irq_frame = 0;
    selected->resume_kind = NOVA_TASK_RESUME_ACTIVE;
    *next_id = selected_id;
    *next_frame = selected_irq_frame;
    task_irq_restore(saved_daif);
    return 1;
}

int nova_task_set_resume_kind(uint32_t task_id, enum nova_task_resume_kind kind) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();

    task = find_task(task_id);
    if (task == 0 ||
        (kind != NOVA_TASK_RESUME_COOPERATIVE &&
         kind != NOVA_TASK_RESUME_IRQ &&
         kind != NOVA_TASK_RESUME_ACTIVE)) {
        task_irq_restore(saved_daif);
        return 0;
    }
    /*
     * FIRST_RUN is creation-only. A cooperative context becomes resumable
     * only after the task has yielded/blocked; an IRQ frame is published
     * only while the interrupted task is still RUNNING.
     */
    if (kind == NOVA_TASK_RESUME_COOPERATIVE &&
        task->state != NOVA_TASK_READY && task->state != NOVA_TASK_BLOCKED) {
        task_irq_restore(saved_daif);
        return 0;
    }
    if ((kind == NOVA_TASK_RESUME_IRQ ||
         kind == NOVA_TASK_RESUME_ACTIVE) &&
        task->state != NOVA_TASK_RUNNING) {
        task_irq_restore(saved_daif);
        return 0;
    }
    task->resume_kind = kind;
    task_irq_restore(saved_daif);
    return 1;
}

static int next_ready_matching(uint32_t after_id, uint32_t *task_id,
                               int resume_path) {
    struct nova_task_info *wrapped = 0;
    struct nova_task_info *next = 0;
    uintptr_t saved_daif = task_irq_save_disable();
    size_t i;
    if (task_id == 0) {
        task_irq_restore(saved_daif);
        return 0;
    }
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
    if (next == 0) {
        task_irq_restore(saved_daif);
        return 0;
    }
    *task_id = next->id;
    task_irq_restore(saved_daif);
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
    uintptr_t saved_daif = task_irq_save_disable();
    if (task != 0 && task->state == NOVA_TASK_RUNNING)
        task->state = NOVA_TASK_ZOMBIE;
    task_irq_restore(saved_daif);
}

int nova_task_set_exit_code(uint32_t task_id, int exit_code) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();
    int result = 0;

    task = find_task(task_id);
    if (task != 0 && task->state == NOVA_TASK_RUNNING) {
        task->exit_code = exit_code;
        result = 1;
    }
    task_irq_restore(saved_daif);
    return result;
}

int nova_task_set_fault(uint32_t task_id, enum nova_task_fault_kind kind,
                        uint64_t esr, uint64_t far, uint64_t elr) {
    struct nova_task_info *task;
    uintptr_t saved_daif = task_irq_save_disable();
    int result = 0;

    task = find_task(task_id);
    if (task != 0 && task->state == NOVA_TASK_RUNNING &&
        kind > NOVA_TASK_FAULT_NONE && kind <= NOVA_TASK_FAULT_BRK) {
        task->fault_kind = kind;
        task->fault_esr = esr;
        task->fault_far = far;
        task->fault_elr = elr;
        task->exit_code = NOVA_TASK_EXIT_FAULT;
        result = 1;
    }
    task_irq_restore(saved_daif);
    return result;
}

int nova_task_reap_zombies(void) {
    uint32_t ids[NOVA_TASK_MAX];
    uintptr_t saved_daif = task_irq_save_disable();
    size_t i, count = 0U, reaped = 0U;

    /* Snapshot IDs atomically; destroy outside this scan's critical section. */
    for (i = 0; i < NOVA_TASK_MAX; ++i) {
        if (tasks[i].state == NOVA_TASK_ZOMBIE)
            ids[count++] = tasks[i].id;
    }
    task_irq_restore(saved_daif);

    for (i = 0; i < count; ++i)
        if (nova_task_destroy(ids[i])) ++reaped;
    return (int)reaped;
}

int nova_task_destroy(uint32_t task_id) {
    struct nova_task_info *task;
    uintptr_t stack;
    uint32_t address_space_id;
    uintptr_t saved_daif = task_irq_save_disable();

    task = find_task(task_id);
    if (task == 0 ||
        (task->state != NOVA_TASK_ZOMBIE &&
         !((task->state == NOVA_TASK_READY ||
            task->state == NOVA_TASK_BLOCKED) &&
           (task->resume_kind == NOVA_TASK_RESUME_FIRST_RUN ||
            task->resume_kind == NOVA_TASK_RESUME_USER_FIRST_RUN))) ||
        task->teardown_in_progress != 0U) {
        task_irq_restore(saved_daif);
        return 0;
    }

    /*
     * Claim teardown before dropping the IRQ mask. The task is no longer
     * schedulable, and a concurrent/re-entrant reaper must not free the same
     * stack or release the same address-space reference twice.
     */
    task->state = NOVA_TASK_ZOMBIE;
    task->teardown_in_progress = 1U;
    stack = task->kernel_stack_base;
    address_space_id = task->address_space_id;
    task_irq_restore(saved_daif);

    /*
     * Stack unmapping may touch allocator/page-table state and is kept outside
     * the task-table critical section. ZOMBIE + teardown_in_progress pins this
     * TCB identity until the operation is committed or retried.
     */
    if (stack != 0U && !nova_vmap_free_guarded_page((void *)stack)) {
        saved_daif = task_irq_save_disable();
        task = find_task(task_id);
        if (task != 0) task->teardown_in_progress = 0U;
        task_irq_restore(saved_daif);
        return 0;
    }

    saved_daif = task_irq_save_disable();
    task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_ZOMBIE ||
        task->teardown_in_progress == 0U ||
        task->kernel_stack_base != stack ||
        task->address_space_id != address_space_id) {
        task_irq_restore(saved_daif);
        /* The stack has been released; an invariant violation is fatal. */
        return 0;
    }
    task->kernel_stack_base = 0U;
    task->kernel_stack_top = 0U;
    task->irq_frame = 0;

    /*
     * These registry cleanups are currently IRQ-serialized and expose no
     * separate locking contract. Keep them in the short metadata section.
     */
    nova_capability_task_exit(task_id);
    nova_service_close_owner(task_id);
    nova_ipc_close_owner(task_id);

    /*
     * The final address-space release may invoke a page-table destroy hook.
     * Drop the task-table IRQ mask first; the teardown claim keeps this TCB
     * pinned and prevents another reaper from consuming its ownership.
     */
    task_irq_restore(saved_daif);
    if (address_space_id != 0U &&
        !nova_address_space_release(address_space_id)) {
        saved_daif = task_irq_save_disable();
        task = find_task(task_id);
        if (task != 0) task->teardown_in_progress = 0U;
        task_irq_restore(saved_daif);
        return 0;
    }

    saved_daif = task_irq_save_disable();
    task = find_task(task_id);
    if (task == 0 || task->state != NOVA_TASK_ZOMBIE ||
        task->teardown_in_progress == 0U ||
        task->kernel_stack_base != 0U ||
        task->address_space_id != address_space_id) {
        task_irq_restore(saved_daif);
        return 0;
    }

    task->id = 0U;
    task->address_space_id = 0U;
    task->state = NOVA_TASK_UNUSED;
    task->resume_kind = NOVA_TASK_RESUME_FIRST_RUN;
    task->entry = 0;
    task->argument = 0;
    task->user_entry = 0U;
    task->user_stack_top = 0U;
    task->kernel_stack_base = 0U;
    task->kernel_stack_top = 0U;
    task->exit_code = 0;
    task->fault_kind = NOVA_TASK_FAULT_NONE;
    task->fault_esr = 0U;
    task->fault_far = 0U;
    task->fault_elr = 0U;
    task->teardown_in_progress = 0U;
    {
        size_t i;
        for (i = 0; i < 12U; ++i) task->context.x19_x30[i] = 0U;
        task->context.sp = 0U;
        task->irq_frame = 0;
    }

    task_irq_restore(saved_daif);
    return 1;
}
