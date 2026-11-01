/* Cooperative AArch64 context switch; SP offset is 96 bytes. */
.section .text, "ax"
.align 4
.global nova_arch_context_switch
.type nova_arch_context_switch, %function
nova_arch_context_switch:
    stp     x19, x20, [x0, #0]
    stp     x21, x22, [x0, #16]
    stp     x23, x24, [x0, #32]
    stp     x25, x26, [x0, #48]
    stp     x27, x28, [x0, #64]
    stp     x29, x30, [x0, #80]
    mov     x2, sp
    str     x2, [x0, #96]
    ldr     x2, [x1, #96]
    mov     sp, x2
    ldp     x19, x20, [x1, #0]
    ldp     x21, x22, [x1, #16]
    ldp     x23, x24, [x1, #32]
    ldp     x25, x26, [x1, #48]
    ldp     x27, x28, [x1, #64]
    ldp     x29, x30, [x1, #80]
    ret
.size nova_arch_context_switch, . - nova_arch_context_switch

/*
 * First EL0 dispatch trampoline: x19 points to the prepared IRQ frame.
 * The cooperative switch has already saved scheduler_context before ERET.
 */
.global nova_user_task_start_trampoline
.type nova_user_task_start_trampoline, %function
nova_user_task_start_trampoline:
    mov     x0, x19
    bl      nova_arch_irq_resume
1:
    wfe
    b       1b
.size nova_user_task_start_trampoline, . - nova_user_task_start_trampoline

/* First-run trampoline: x19=entry, x20=argument, x21=task control block. */
.extern nova_sched_task_exit
.global nova_task_start_trampoline
.type nova_task_start_trampoline, %function
nova_task_start_trampoline:
    /* First entry uses RET, not ERET; enable IRQs for task execution. */
    msr     daifclr, #2
    mov     x0, x20
    blr     x19
    mov     x0, x21
    bl      nova_task_mark_zombie
    bl      nova_sched_task_exit
1:
    wfe
    b       1b
.size nova_task_start_trampoline, . - nova_task_start_trampoline

/*
 * Restore a complete IRQ frame selected by the scheduler.
 * x0 points at the 272-byte frame on the selected task's kernel stack;
 * SP_EL0 is restored as part of the task's user execution state.
 * The frame carries the interrupted ELR/SPSR; ERET restores the task's
 * original PSTATE and execution point. This routine does not return.
 */
.global nova_arch_irq_resume
.type nova_arch_irq_resume, %function
nova_arch_irq_resume:
    mov     sp, x0
    ldr     x16, [sp, #256]
    msr     SPSR_EL1, x16
    ldr     x16, [sp, #248]
    msr     ELR_EL1, x16
    ldr     x16, [sp, #264]
    msr     SP_EL0, x16
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    ldp x0, x1, [sp, #0]
    add     sp, sp, #272
    eret
.size nova_arch_irq_resume, . - nova_arch_irq_resume
