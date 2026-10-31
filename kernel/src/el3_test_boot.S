/*
 * EL3 test firmware: enter non-secure EL2h and transfer control to the
 * kernel ELF at 0x40080000. The kernel is expected to drop to EL1h.
 */
.section .text.boot, "ax"
.global _el3_test_start
.type _el3_test_start, %function
_el3_test_start:
    msr     DAIFSet, #0xf
    mrs     x1, SCR_EL3
    orr     x1, x1, #1
    orr     x1, x1, #0x400
    orr     x1, x1, #0x100
    msr     SCR_EL3, x1
    mov     x1, #0x3c9
    msr     SPSR_EL3, x1
    ldr     x1, =0x40080000
    msr     ELR_EL3, x1
    ldr     x0, =0x41000000
    isb
    eret
.size _el3_test_start, . - _el3_test_start
