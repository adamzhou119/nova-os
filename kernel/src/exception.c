#include "exception.h"

#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK  0x3fU
#define ESR_ISS_FSC_MASK 0x3fU
#define ESR_ISS_WNR (1U << 6)
#define ESR_ISS_S1PTW (1U << 7)
#define ESR_ISS_FNV (1U << 10)
#define SPSR_MODE_MASK 0x0fU
#define SPSR_MODE_EL0T 0x00U

static uint64_t exception_class(uint64_t esr) {
    return (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
}

const char *nova_exception_kind(uint64_t vector_slot) {
    if (vector_slot >= 16U) return "Unknown vector";
    switch (vector_slot & 3U) {
    case 0U: return "Synchronous";
    case 1U: return "IRQ";
    case 2U: return "FIQ";
    case 3U: return "SError";
    default: return "Unknown";
    }
}

const char *nova_exception_class_name(uint64_t esr_el1) {
    switch (exception_class(esr_el1)) {
    case 0x00U: return "Unknown reason";
    case 0x01U: return "Trapped WFI or WFE";
    case 0x0eU: return "Illegal execution state";
    case 0x15U: return "AArch64 SVC";
    case 0x18U: return "Trapped MSR or MRS";
    case 0x20U: return "Instruction abort from lower EL";
    case 0x21U: return "Instruction abort from same EL";
    case 0x22U: return "PC alignment fault";
    case 0x24U: return "Data abort from lower EL";
    case 0x25U: return "Data abort from same EL";
    case 0x26U: return "SP alignment fault";
    case 0x2fU: return "SError interrupt";
    case 0x30U: return "Breakpoint from lower EL";
    case 0x31U: return "Breakpoint from same EL";
    case 0x32U: return "Software step from lower EL";
    case 0x33U: return "Software step from same EL";
    case 0x34U: return "Watchpoint from lower EL";
    case 0x35U: return "Watchpoint from same EL";
    case 0x3cU: return "AArch64 BRK instruction";
    default: return "Unrecognized exception class";
    }
}

enum nova_exception_policy nova_exception_policy_for(
    uint64_t vector_slot, uint64_t esr_el1, uint64_t spsr_el1) {
    uint64_t ec = exception_class(esr_el1);
    int from_el0 = (spsr_el1 & SPSR_MODE_MASK) == SPSR_MODE_EL0T;

    /* Only synchronous exceptions from EL0 are eligible for task containment. */
    if (vector_slot != 8U || !from_el0)
        return NOVA_EXCEPTION_POLICY_KERNEL_FATAL;

    if (ec == 0x15U) return NOVA_EXCEPTION_POLICY_USER_SVC;
    if (ec == 0x20U || ec == 0x24U || ec == 0x30U ||
        ec == 0x32U || ec == 0x34U || ec == 0x3cU)
        return NOVA_EXCEPTION_POLICY_USER_FAULT;

    /* Same-EL aborts, SError, illegal state and unknown traps stay fatal. */
    return NOVA_EXCEPTION_POLICY_KERNEL_FATAL;
}

int nova_exception_is_instruction_abort(uint64_t esr) {
    uint64_t ec = exception_class(esr);
    return ec == 0x20U || ec == 0x21U;
}

int nova_exception_is_data_abort(uint64_t esr) {
    uint64_t ec = exception_class(esr);
    return ec == 0x24U || ec == 0x25U;
}

int nova_exception_is_write_abort(uint64_t esr) {
    return nova_exception_is_data_abort(esr) && (esr & ESR_ISS_WNR) != 0U;
}

int nova_exception_fault_address_valid(uint64_t esr) {
    return (nova_exception_is_instruction_abort(esr) ||
            nova_exception_is_data_abort(esr)) &&
           (esr & ESR_ISS_FNV) == 0U;
}

int nova_exception_is_stage1_page_walk(uint64_t esr) {
    return (nova_exception_is_instruction_abort(esr) ||
            nova_exception_is_data_abort(esr)) &&
           (esr & ESR_ISS_S1PTW) != 0U;
}

const char *nova_abort_fault_status_name(uint64_t esr) {
    uint64_t ec = exception_class(esr);
    uint64_t fsc = esr & ESR_ISS_FSC_MASK;

    if (ec != 0x20U && ec != 0x21U && ec != 0x24U && ec != 0x25U)
        return "Not an instruction/data abort";

    if (fsc <= 0x03U) return "Address size fault";
    if (fsc >= 0x04U && fsc <= 0x07U) return "Translation fault";
    if (fsc >= 0x08U && fsc <= 0x0bU) return "Access flag fault";
    if (fsc >= 0x0cU && fsc <= 0x0fU) return "Permission fault";
    if (fsc == 0x10U) return "Synchronous external abort";
    if (fsc == 0x11U) return "Synchronous tag check fault";
    if (fsc >= 0x14U && fsc <= 0x17U)
        return "External abort during translation table walk";
    if (fsc == 0x18U) return "Synchronous parity/ECC error";
    if (fsc >= 0x1cU && fsc <= 0x1fU)
        return "Parity/ECC error during translation table walk";
    if (fsc == 0x21U) return "Alignment fault";
    if (fsc == 0x22U) return "Debug event";
    if (fsc == 0x30U) return "TLB conflict abort";
    return "Reserved or implementation-defined fault status";
}
