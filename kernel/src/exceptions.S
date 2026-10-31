/*
 * Nova OS — EL1 exception vector table and terminal diagnostic entry.
 *
 * Each slot records its vector index in x16 before entering the common
 * non-returning handler. This is diagnostic scaffolding, not exception return.
 */
.section .text.vectors, "ax"
.balign 2048
.global exception_vectors_el1
.type exception_vectors_el1, %object

.macro VECTOR slot
    mov     x16, #\slot
    b       exception_vector_panic
    .space  120
.endm

exception_vectors_el1:
    /* Current EL with SP0: synchronous, IRQ, FIQ, SError. */
    VECTOR 0
    VECTOR 1
    VECTOR 2
    VECTOR 3

    /* Current EL with SPx: synchronous, IRQ, FIQ, SError. */
    VECTOR 4
    VECTOR 5
    VECTOR 6
    VECTOR 7

    /* Lower EL using AArch64: synchronous, IRQ, FIQ, SError. */
    VECTOR 8
    VECTOR 9
    VECTOR 10
    VECTOR 11

    /* Lower EL using AArch32: synchronous, IRQ, FIQ, SError. */
    VECTOR 12
    VECTOR 13
    VECTOR 14
    VECTOR 15

.size exception_vectors_el1, . - exception_vectors_el1

.section .text, "ax"
.global exception_vector_panic
.type exception_vector_panic, %function
exception_vector_panic:
    /* No exception return yet; keep the stack 16-byte aligned. */
    sub     sp, sp, #16
    mrs     x0, ESR_EL1
    mrs     x1, ELR_EL1
    mov     x2, x16
    bl      exception_report

1:
    wfe
    b       1b

.size exception_vector_panic, . - exception_vector_panic
