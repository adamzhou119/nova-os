/*
 * Nova OS — EL1 exception vector table and terminal diagnostic entry.
 *
 * Every vector currently enters a common non-returning handler. This is an
 * early diagnostic implementation, not a scheduler-aware exception path.
 */
.section .text.vectors, "ax"
.balign 2048
.global exception_vectors_el1
.type exception_vectors_el1, %object

.macro VECTOR
    b       exception_vector_panic
    .space  124
.endm

exception_vectors_el1:
    /* Current EL with SP0: synchronous, IRQ, FIQ, SError. */
    VECTOR
    VECTOR
    VECTOR
    VECTOR

    /* Current EL with SPx: synchronous, IRQ, FIQ, SError. */
    VECTOR
    VECTOR
    VECTOR
    VECTOR

    /* Lower EL using AArch64: synchronous, IRQ, FIQ, SError. */
    VECTOR
    VECTOR
    VECTOR
    VECTOR

    /* Lower EL using AArch32: synchronous, IRQ, FIQ, SError. */
    VECTOR
    VECTOR
    VECTOR
    VECTOR

.size exception_vectors_el1, . - exception_vectors_el1

.section .text, "ax"
.global exception_vector_panic
.type exception_vector_panic, %function
exception_vector_panic:
    /* No exception return yet: preserve stack alignment and report context. */
    sub     sp, sp, #16
    stp     x0, x1, [sp]
    mrs     x0, ESR_EL1
    mrs     x1, ELR_EL1
    bl      exception_report

1:
    wfe
    b       1b

.size exception_vector_panic, . - exception_vector_panic
