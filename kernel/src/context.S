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
