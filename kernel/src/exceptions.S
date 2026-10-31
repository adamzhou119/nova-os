/*
 * Nova OS — AArch64 EL1 exception vectors.
 *
 * IRQ entry preserves the complete integer register set plus ELR_EL1 and
 * SPSR_EL1. The 272-byte frame remains 16-byte aligned for the C dispatcher.
 */
.section .text.vectors, "ax"
.balign 2048
.global exception_vectors_el1
.type exception_vectors_el1, %object

/* Do not use a GPR to encode the vector slot: interrupted x16 must survive. */
.macro IRQ_VECTOR
    b       exception_irq_entry
    .space  124
.endm

.macro VECTOR slot
    mov     x16, #\slot
    b       exception_vector_panic
    .space  120
.endm

exception_vectors_el1:
    VECTOR 0
    VECTOR 1
    VECTOR 2
    VECTOR 3
    VECTOR 4
    IRQ_VECTOR
    VECTOR 6
    VECTOR 7
    VECTOR 8
    VECTOR 9
    VECTOR 10
    VECTOR 11
    VECTOR 12
    VECTOR 13
    VECTOR 14
    VECTOR 15

/* Every AArch64 vector table has 16 fixed 128-byte slots. */
.if (. - exception_vectors_el1) != 2048
    .error "EL1 exception vector table must contain exactly 16 x 128-byte slots"
.endif
.size exception_vectors_el1, . - exception_vectors_el1

/*
 * Current EL SPx IRQ frame:
 *   x0..x30: offsets 0..240
 *   ELR_EL1: offset 248
 *   SPSR_EL1: offset 256
 */
.global exception_irq_entry
.type exception_irq_entry, %function
exception_irq_entry:
    sub     sp, sp, #272
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]
    mrs     x16, ELR_EL1
    str     x16, [sp, #248]
    mrs     x16, SPSR_EL1
    str     x16, [sp, #256]
    mov     x0, sp
    bl      nova_irq_dispatch
    mov     sp, x0
    ldr     x16, [sp, #256]
    msr     SPSR_EL1, x16
    ldr     x16, [sp, #248]
    msr     ELR_EL1, x16
    ldr x30, [sp, #240]
    ldp x28, x29, [sp, #224]
    ldp x26, x27, [sp, #208]
    ldp x24, x25, [sp, #192]
    ldp x22, x23, [sp, #176]
    ldp x20, x21, [sp, #160]
    ldp x18, x19, [sp, #144]
    ldp x16, x17, [sp, #128]
    ldp x14, x15, [sp, #112]
    ldp x12, x13, [sp, #96]
    ldp x10, x11, [sp, #80]
    ldp x8, x9, [sp, #64]
    ldp x6, x7, [sp, #48]
    ldp x4, x5, [sp, #32]
    ldp x2, x3, [sp, #16]
    ldp x0, x1, [sp, #0]
    add sp, sp, #272
    eret
.size exception_irq_entry, . - exception_irq_entry

.section .text, "ax"
.global exception_vector_panic
.type exception_vector_panic, %function
exception_vector_panic:
    /* The handler is terminal; retain ABI stack alignment. */
    sub     sp, sp, #16
    mrs     x0, ESR_EL1
    mrs     x1, ELR_EL1
    mov     x2, x16
    mrs     x3, FAR_EL1
    mrs     x4, SPSR_EL1
    bl      exception_report

1:
    wfe
    b       1b

.size exception_vector_panic, . - exception_vector_panic
