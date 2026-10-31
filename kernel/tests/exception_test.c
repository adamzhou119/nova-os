#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "exception.h"

static uint64_t esr_for_ec(uint64_t ec) { return ec << 26; }

int main(void) {
    assert(strcmp(nova_exception_kind(0U), "Synchronous") == 0);
    assert(strcmp(nova_exception_kind(4U), "Synchronous") == 0);
    assert(strcmp(nova_exception_kind(5U), "IRQ") == 0);
    assert(strcmp(nova_exception_kind(10U), "FIQ") == 0);
    assert(strcmp(nova_exception_kind(15U), "SError") == 0);
    assert(strcmp(nova_exception_kind(16U), "Unknown vector") == 0);

    assert(strcmp(nova_exception_class_name(esr_for_ec(0x15U)),
                  "AArch64 SVC") == 0);
    assert(strcmp(nova_exception_class_name(esr_for_ec(0x24U)),
                  "Data abort from lower EL") == 0);
    assert(strcmp(nova_exception_class_name(esr_for_ec(0x2fU)),
                  "SError interrupt") == 0);
    assert(strcmp(nova_exception_class_name(esr_for_ec(0x3cU)),
                  "AArch64 BRK instruction") == 0);
    assert(strcmp(nova_exception_class_name(esr_for_ec(0x3fU)),
                  "Unrecognized exception class") == 0);

    puts("Exception classification host tests passed.");
    return 0;
}
