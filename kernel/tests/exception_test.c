#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "exception.h"

#define ESR_EC(ec) ((uint64_t)(ec) << 26)
#define ESR_FSC(fsc) ((uint64_t)(fsc))
#define ESR_WNR (1ULL << 6)
#define ESR_S1PTW (1ULL << 7)
#define ESR_FNV (1ULL << 10)
#define SPSR_EL0T 0U
#define SPSR_EL1H 5U

int main(void) {
    uint64_t data_write = ESR_EC(0x24U) | ESR_FSC(0x0dU) | ESR_WNR;
    uint64_t instruction_translation = ESR_EC(0x20U) | ESR_FSC(0x05U);

    assert(strcmp(nova_exception_kind(0U), "Synchronous") == 0);
    assert(strcmp(nova_exception_kind(4U), "Synchronous") == 0);
    assert(strcmp(nova_exception_kind(5U), "IRQ") == 0);
    assert(strcmp(nova_exception_kind(10U), "FIQ") == 0);
    assert(strcmp(nova_exception_kind(15U), "SError") == 0);
    assert(strcmp(nova_exception_kind(16U), "Unknown vector") == 0);

    assert(strcmp(nova_exception_class_name(ESR_EC(0x15U)),
                  "AArch64 SVC") == 0);
    assert(strcmp(nova_exception_class_name(ESR_EC(0x24U)),
                  "Data abort from lower EL") == 0);
    assert(strcmp(nova_exception_class_name(ESR_EC(0x2fU)),
                  "SError interrupt") == 0);
    assert(strcmp(nova_exception_class_name(ESR_EC(0x3cU)),
                  "AArch64 BRK instruction") == 0);
    assert(strcmp(nova_exception_class_name(ESR_EC(0x3fU)),
                  "Unrecognized exception class") == 0);

    /* Only lower-EL synchronous exceptions are eligible for containment. */
    assert(nova_exception_policy_for(8U, ESR_EC(0x15U), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_USER_SVC);
    assert(nova_exception_policy_for(8U, ESR_EC(0x24U), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_USER_FAULT);
    assert(nova_exception_policy_for(8U, ESR_EC(0x20U), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_USER_FAULT);
    assert(nova_exception_policy_for(8U, ESR_EC(0x3cU), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_USER_FAULT);
    assert(nova_exception_policy_for(8U, ESR_EC(0x25U), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_KERNEL_FATAL);
    assert(nova_exception_policy_for(8U, ESR_EC(0x15U), SPSR_EL1H) ==
           NOVA_EXCEPTION_POLICY_KERNEL_FATAL);
    assert(nova_exception_policy_for(5U, ESR_EC(0x24U), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_KERNEL_FATAL);
    assert(nova_exception_policy_for(8U, ESR_EC(0x3fU), SPSR_EL0T) ==
           NOVA_EXCEPTION_POLICY_KERNEL_FATAL);

    assert(nova_exception_is_data_abort(data_write));
    assert(!nova_exception_is_instruction_abort(data_write));
    assert(nova_exception_is_write_abort(data_write));
    assert(nova_exception_fault_address_valid(data_write));
    assert(strcmp(nova_abort_fault_status_name(data_write),
                  "Permission fault") == 0);

    assert(nova_exception_is_instruction_abort(instruction_translation));
    assert(!nova_exception_is_data_abort(instruction_translation));
    assert(!nova_exception_is_write_abort(instruction_translation));
    assert(strcmp(nova_abort_fault_status_name(instruction_translation),
                  "Translation fault") == 0);

    assert(nova_exception_is_stage1_page_walk(
        ESR_EC(0x25U) | ESR_FSC(0x04U) | ESR_S1PTW));
    assert(!nova_exception_fault_address_valid(
        ESR_EC(0x25U) | ESR_FSC(0x04U) | ESR_FNV));
    assert(!nova_exception_is_write_abort(ESR_EC(0x15U) | ESR_WNR));
    assert(strcmp(nova_abort_fault_status_name(ESR_EC(0x15U)),
                  "Not an instruction/data abort") == 0);
    assert(strcmp(nova_abort_fault_status_name(ESR_EC(0x25U) | 0x3fU),
                  "Reserved or implementation-defined fault status") == 0);

    puts("Exception classification, recovery policy, and abort syndrome tests passed.");
    return 0;
}
