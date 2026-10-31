/*
 * Nova OS — AArch64 experimental entry point.
 * QEMU virt bring-up scaffold; boot assumptions require runtime validation.
 */
.section .text.boot
.global _start
.type _start, %function

_start:
    /* Establish a private stack for the primary CPU. */
    ldr     x0, =_stack_top
    mov     sp, x0

    /*
     * Clear .bss before entering C. The stack is inside .bss, but no stack
     * memory is used between setting SP and completing this loop.
     */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
1:
    cmp     x1, x2
    b.hs    2f
    str     xzr, [x1], #8
    b       1b
2:
    dsb     sy
    isb

    /* Park secondary CPUs until per-CPU stacks and SMP support exist. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbz     x0, 3f
4:
    wfe
    b       4b

3:
    bl      kernel_main

5:
    wfe
    b       5b

.size _start, . - _start

.section .bss.stack, "aw", %nobits
.align 12
_stack_bottom:
    .skip 16384
_stack_top:
