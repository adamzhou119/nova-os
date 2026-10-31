/*
 * Test firmware accepts a QEMU secure reset at EL3 or an EL2 reset, then
 * transfers control to the EL2 test kernel at 0x40200000. The kernel must
 * enter EL1h before calling C.
 */
.section .text.boot, "ax"
.global _el3_test_start
.type _el3_test_start, %function
_el3_test_start:
    msr     DAIFSet, #0xf
    ldr     x3, =0x09000000
    mov     w4, #'E'
    str     w4, [x3]
    mrs     x2, CurrentEL
    lsr     x2, x2, #2
    add     w4, w2, #'0'
    str     w4, [x3]
    cmp     x2, #3
    b.eq    1f
    cmp     x2, #2
    b.eq    2f
3:
    wfe
    b       3b

/* Secure firmware entry: configure a non-secure AArch64 EL2 target. */
1:
    mrs     x1, SCR_EL3
    orr     x1, x1, #1
    orr     x1, x1, #0x400
    orr     x1, x1, #0x100
    msr     SCR_EL3, x1
    mov     x1, #0x3c9
    msr     SPSR_EL3, x1
    adr     x1, 2f
    msr     ELR_EL3, x1
    ldr     x0, =0x40000000
    isb
    eret

/* Enter the kernel at EL2; boot.S is responsible for the EL2-to-EL1 drop. */
2:
    /* Match the DTB address used by the CI QEMU loader. */
    ldr     x0, =0x41000000
    ldr     x1, =0x40200000
    br      x1
.size _el3_test_start, . - _el3_test_start
