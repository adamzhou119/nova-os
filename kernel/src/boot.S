/*
 * Nova OS — AArch64 experimental entry point.
 * QEMU virt bring-up scaffold; boot assumptions require runtime validation.
 */
.section .text.boot
.global _start
.type _start, %function

_start:
    /* Preserve the QEMU/firmware DTB pointer passed in x0. */
    mov     x20, x0

    /* Establish the bootstrap stack before using any memory. */
    ldr     x0, =_stack_top
    mov     sp, x0

    /*
     * Only the primary CPU may initialize shared .bss. Secondary CPUs park
     * before touching the shared stack or the initialization range.
     */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbz     x0, 1f
2:
    wfe
    b       2b

1:
    /*
     * Clear .bss before entering C. The stack is inside .bss, but no stack
     * memory is used between setting SP and completing this loop.
     */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
3:
    cmp     x1, x2
    b.hs    4f
    str     xzr, [x1], #8
    b       3b
4:
    dsb     sy
    isb

    /* Pass the decoded current exception level to kernel_main. */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2

    /* The current prototype installs vectors only when running at EL1. */
    cmp     x0, #1
    b.ne    6f
    ldr     x1, =exception_vectors_el1
    msr     VBAR_EL1, x1
    isb
6:
    /* Pass the original DTB pointer after vector setup (which uses x1). */
    mov     x1, x20
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
