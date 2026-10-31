/*
 * Nova OS — AArch64 experimental entry point.
 * QEMU virt bring-up scaffold; boot assumptions require runtime validation.
 */
.section .text.boot
.global _start
.type _start, %function

_start:
    ldr     x0, =_stack_top
    mov     sp, x0

    /* Park secondary CPUs until SMP support is implemented. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff
    cbz     x0, 1f
0:
    wfe
    b       0b

1:
    bl      kernel_main

2:
    wfe
    b       2b

.size _start, . - _start

.section .bss.stack, "aw", %nobits
.align 12
_stack_bottom:
    .skip 16384
_stack_top:
