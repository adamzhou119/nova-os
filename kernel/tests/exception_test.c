#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "exception.h"

static uint64_t esr_for_ec(uint64_t ec) { return ec << 26; }

int main(void) {
    assert(nova_exception_kind(0U)[0] == 'S');
    assert(nova_exception_kind(4U)[0] == 'S');
    assert(nova_exception_kind(5U)[0] == 'I');
    assert(nova_exception_kind(10U)[0] == 'F');
    assert(nova_exception_kind(15U)[0] == 'S');
    assert(nova_exception_class_name(esr_for_ec(0x15U))[0] == 'A');
    assert(nova_exception_class_name(esr_for_ec(0x24U))[0] == 'D');
    assert(nova_exception_class_name(esr_for_ec(0x2fU))[0] == 'S');
    assert(nova_exception_class_name(esr_for_ec(0x3cU))[0] == 'A');
    assert(nova_exception_class_name(esr_for_ec(0x3fU))[0] == 'U');
    puts("Exception classification host tests passed.");
    return 0;
}
