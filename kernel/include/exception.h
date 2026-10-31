#ifndef NOVA_EXCEPTION_H
#define NOVA_EXCEPTION_H

#include <stdint.h>

enum nova_exception_policy {
    NOVA_EXCEPTION_POLICY_KERNEL_FATAL = 0,
    NOVA_EXCEPTION_POLICY_USER_FAULT,
    NOVA_EXCEPTION_POLICY_USER_SVC
};

/* Architectural vector-slot class, independent of the exception syndrome. */
const char *nova_exception_kind(uint64_t vector_slot);

/* Human-readable ESR_EL1.EC decoding; unknown values remain explicit. */
const char *nova_exception_class_name(uint64_t esr_el1);

/*
 * Conservative EL1 policy decision. This classifies a trap only; it does not
 * resume or terminate a task. EL0 faults are candidates for task containment.
 */
enum nova_exception_policy nova_exception_policy_for(
    uint64_t vector_slot, uint64_t esr_el1, uint64_t spsr_el1);

/* Abort syndrome helpers; return false/neutral for non-abort exception classes. */
int nova_exception_is_instruction_abort(uint64_t esr);
int nova_exception_is_data_abort(uint64_t esr);
int nova_exception_is_write_abort(uint64_t esr);
int nova_exception_fault_address_valid(uint64_t esr);
int nova_exception_is_stage1_page_walk(uint64_t esr);
const char *nova_abort_fault_status_name(uint64_t esr);

#endif
