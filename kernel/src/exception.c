#include "exception.h"

const char *nova_exception_kind(uint64_t vector_slot) {
    switch (vector_slot & 3U) {
    case 0U: return "Synchronous";
    case 1U: return "IRQ";
    case 2U: return "FIQ";
    case 3U: return "SError";
    default: return "Unknown";
    }
}

const char *nova_exception_class_name(uint64_t esr_el1) {
    switch ((esr_el1 >> 26) & 0x3fU) {
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
