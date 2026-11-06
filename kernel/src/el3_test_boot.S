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
    /* Secure test firmware owns a terminal EL3 exception vector table. */
    ldr     x1, =el3_exception_vectors
    msr     VBAR_EL3, x1
    isb

    /*
     * Initialize QEMU virt PL011 before emitting the firmware entry marker.
     * The secure boot fixture runs before the non-secure kernel can configure
     * the UART, so it must establish its own 24 MHz / 115200 8N1 settings.
     */
    ldr     x3, =0x09000000
    str     wzr, [x3, #0x30]       /* UARTCR: disable before reconfiguration. */
    mov     w5, #13
    str     w5, [x3, #0x24]       /* UARTIBRD. */
    mov     w5, #1
    str     w5, [x3, #0x28]       /* UARTFBRD. */
    mov     w5, #0x70
    str     w5, [x3, #0x2c]       /* UARTLCR_H: 8-bit, FIFO enabled. */
    mov     w5, #0x301
    str     w5, [x3, #0x30]       /* UARTCR: UARTEN | TXE | RXE. */
    dsb     sy

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
    /* SCR_EL3 bits [5:4] are RES1 on the Cortex-A57 profile. */
    orr     x1, x1, #0x30
    orr     x1, x1, #1
    orr     x1, x1, #0x400
    orr     x1, x1, #0x100
    msr     SCR_EL3, x1
    isb
    mov     x1, #0x3c9
    msr     SPSR_EL3, x1
    adr     x1, 2f
    msr     ELR_EL3, x1
    ldr     x0, =0x40000000
    isb
    eret

/* Enter the kernel at EL2; boot.S is responsible for the EL2-to-EL1 drop. */
2:
    /* QEMU secure virt places the generated DTB at the start of RAM. */
    ldr     x0, =0x40000000
    ldr     x1, =0x40200000
    br      x1
.size _el3_test_start, . - _el3_test_start

/*
 * The EL3 test firmware has no secure-monitor recovery services yet.
 * Every vector enters a terminal halt rather than executing an undefined
 * firmware path. The table is kept separate from the non-secure EL1 kernel.
 */
.section .text.vectors.el3, "ax"
.balign 2048
.global el3_exception_vectors
.type el3_exception_vectors, %object
.macro EL3_VECTOR slot
    mov     x16, #\slot
    b       el3_exception_halt
    .space  120
.endm

el3_exception_vectors:
    EL3_VECTOR 0
    EL3_VECTOR 1
    EL3_VECTOR 2
    EL3_VECTOR 3
    EL3_VECTOR 4
    EL3_VECTOR 5
    EL3_VECTOR 6
    EL3_VECTOR 7
    EL3_VECTOR 8
    EL3_VECTOR 9
    EL3_VECTOR 10
    EL3_VECTOR 11
    EL3_VECTOR 12
    EL3_VECTOR 13
    EL3_VECTOR 14
    EL3_VECTOR 15

.if (. - el3_exception_vectors) != 2048
    .error "EL3 exception vector table must contain exactly 16 x 128-byte slots"
.endif
.size el3_exception_vectors, . - el3_exception_vectors

.section .text, "ax"
.type el3_exception_halt, %function
el3_exception_halt:
    msr     DAIFSet, #0xf
1:
    wfe
    b       1b
.size el3_exception_halt, . - el3_exception_halt
