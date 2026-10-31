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
    /* QEMU's bare-metal ELF loader leaves x0 unset; CI loads DTB here. */
    cbnz    x20, 7f
    ldr     x20, =0x41000000
7:

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

    /* Decode the current exception level before entering C. */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #2
    b.ne    6f

    /*
     * Firmware may enter at EL2. Drop to AArch64 EL1h with IRQ/FIQ/SError
     * masked. Give EL1 its own bootstrap stack and a known MMU-off SCTLR.
     */
    mov     x1, #1
    lsl     x1, x1, #31
    msr     HCR_EL2, x1
    mov     x1, #3
    msr     CNTHCTL_EL2, x1
    msr     CNTVOFF_EL2, xzr
    /* Install diagnostic vectors at EL2 and target EL1 before ERET. */
    ldr     x1, =exception_vectors_el2
    msr     VBAR_EL2, x1
    ldr     x1, =exception_vectors_el1
    msr     VBAR_EL1, x1
    isb
    ldr     x1, =_stack_top
    msr     SP_EL1, x1
    movz    x1, #0x0800
    movk    x1, #0x30d0, lsl #16
    msr     SCTLR_EL1, x1
    mov     x1, #0x3c5
    msr     SPSR_EL2, x1
    adr     x1, 8f
    msr     ELR_EL2, x1
    isb
    eret
8:
    mov     x0, #1

    /* The current prototype installs vectors only when running at EL1. */
6:
    cmp     x0, #1
    b.ne    9f
    ldr     x1, =exception_vectors_el1
    msr     VBAR_EL1, x1
    isb
9:
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
