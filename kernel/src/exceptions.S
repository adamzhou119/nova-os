/*
 * Nova OS — AArch64 EL1 exception vectors.
 *
 * IRQ entry preserves the complete integer register set plus ELR_EL1 and
 * SPSR_EL1 and SP_EL0. The 272-byte frame remains 16-byte aligned.
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

/* Lower-EL AArch64 synchronous exceptions preserve every user GPR. */
.macro USER_SYNC_VECTOR
    b       exception_user_sync_entry
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
    USER_SYNC_VECTOR
    IRQ_VECTOR
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
    mrs     x16, SP_EL0
    str     x16, [sp, #264]
    mov     x0, sp
    bl      nova_irq_dispatch
    mov     sp, x0
    ldr     x16, [sp, #256]
    msr     SPSR_EL1, x16
    ldr     x16, [sp, #248]
    msr     ELR_EL1, x16
    ldr     x16, [sp, #264]
    msr     SP_EL0, x16
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


/*
 * Lower-EL AArch64 synchronous entry. SP_EL1 is the current task's
 * privileged stack; preserve the complete user state before calling C.
 * Frame offsets are defined by include/user_context.h (288 bytes).
 */
.global exception_user_sync_entry
.type exception_user_sync_entry, %function
exception_user_sync_entry:
    sub     sp, sp, #288
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
    mrs     x16, SP_EL0
    str     x16, [sp, #264]
    mrs     x16, ESR_EL1
    str     x16, [sp, #272]
    mrs     x16, FAR_EL1
    str     x16, [sp, #280]
    mov     x0, sp
    bl      nova_user_sync_dispatch
    b       nova_arch_user_sync_resume
.size exception_user_sync_entry, . - exception_user_sync_entry

.section .text, "ax"
/* Restore a complete 288-byte EL0 trap frame and return with ERET. */
.global nova_arch_user_sync_resume
.type nova_arch_user_sync_resume, %function
nova_arch_user_sync_resume:
    mov     sp, x0
    ldr     x16, [sp, #256]
    msr     SPSR_EL1, x16
    ldr     x16, [sp, #248]
    msr     ELR_EL1, x16
    ldr     x16, [sp, #264]
    msr     SP_EL0, x16
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    ldp x0, x1, [sp, #0]
    add     sp, sp, #288
    eret
.size nova_arch_user_sync_resume, . - nova_arch_user_sync_resume

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

/*
 * EL2 vector table protects the short firmware-to-kernel transition window.
 * The handlers are terminal diagnostics; they do not provide hypervisor
 * services or recover from EL2 faults.
 */
.section .text.vectors.el2, "ax"
.balign 2048
.global exception_vectors_el2
.type exception_vectors_el2, %object
.macro VECTOR_EL2 slot
    mov     x16, #\slot
    b       exception_vector_panic_el2
    .space  120
.endm

exception_vectors_el2:
    VECTOR_EL2 0
    VECTOR_EL2 1
    VECTOR_EL2 2
    VECTOR_EL2 3
    VECTOR_EL2 4
    VECTOR_EL2 5
    VECTOR_EL2 6
    VECTOR_EL2 7
    VECTOR_EL2 8
    VECTOR_EL2 9
    VECTOR_EL2 10
    VECTOR_EL2 11
    VECTOR_EL2 12
    VECTOR_EL2 13
    VECTOR_EL2 14
    VECTOR_EL2 15

.if (. - exception_vectors_el2) != 2048
    .error "EL2 exception vector table must contain exactly 16 x 128-byte slots"
.endif
.size exception_vectors_el2, . - exception_vectors_el2

.section .text, "ax"
.global exception_vector_panic_el2
.type exception_vector_panic_el2, %function
exception_vector_panic_el2:
    sub     sp, sp, #16
    mrs     x0, ESR_EL2
    mrs     x1, ELR_EL2
    mov     x2, x16
    mrs     x3, FAR_EL2
    mrs     x4, SPSR_EL2
    bl      exception_report_el2
1:
    wfe
    b       1b
.size exception_vector_panic_el2, . - exception_vector_panic_el2

/* Tiny EL0 test payload: get task id, publish it, then exit via SVC. */
.section .text.user_test, "ax"
.balign 4
.global nova_user_test_payload_start
.global nova_user_test_payload_end
.type nova_user_test_payload_start, %function
nova_user_test_payload_start:
    mov     x8, #0
    svc     #0
    str     x0, [sp, #-16]!
    adr     x0, 2f
    mov     x1, #15
    mov     x8, #2
    svc     #0
    movz    x0, #0x8000, lsl #16
    add     x0, x0, #0x4000
    mov     x1, #1
    mov     x8, #2
    svc     #0
    cmn     x0, #14
    b.ne    1f
    movz    x0, #0x8000, lsl #16
    add     x0, x0, #0x2000
    add     x0, x0, #0x0ff8
    mov     x1, #16
    mov     x8, #2
    svc     #0
    cmn     x0, #14
    b.ne    1f
    adr     x0, 2f
    mov     x1, #129
    mov     x8, #2
    svc     #0
    cmn     x0, #22
    b.ne    1f
    mov     x0, #0
    mov     x8, #1
    svc     #0
1:
    b       1b
2:
    .ascii  "hello from EL0\n"
nova_user_test_payload_end:
.size nova_user_test_payload_start, nova_user_test_payload_end - nova_user_test_payload_start
/* Deliberately read an unmapped EL0 address to exercise task fault containment. */
.section .text.user_fault_test, "ax"
.balign 4
.global nova_user_fault_payload_start
.global nova_user_fault_payload_end
.type nova_user_fault_payload_start, %function
nova_user_fault_payload_start:
    movz    x0, #0x8000, lsl #16
    add     x0, x0, #6, lsl #12
    ldr     x1, [x0]
1:
    b       1b
nova_user_fault_payload_end:
.size nova_user_fault_payload_start, nova_user_fault_payload_end - nova_user_fault_payload_start
/* A healthy peer publishes its task ID and exits after another user task faults. */
.section .text.user_healthy_test, "ax"
.balign 4
.global nova_user_healthy_payload_start
.global nova_user_healthy_payload_end
.type nova_user_healthy_payload_start, %function
nova_user_healthy_payload_start:
    mov     x8, #0
    svc     #0
    str     x0, [sp, #-16]!
    mov     x8, #1
    mov     x0, #0
    svc     #0
1:
    b       1b
nova_user_healthy_payload_end:
.size nova_user_healthy_payload_start, nova_user_healthy_payload_end - nova_user_healthy_payload_start


