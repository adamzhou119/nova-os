#include "user_context.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    struct nova_user_trap_frame frame;
    size_t i;

    memset(&frame, 0, sizeof(frame));
    for (i = 0; i < 31U; ++i) frame.x[i] = 0x1000U + i;
    frame.elr_el1 = 0x400000U;
    frame.spsr_el1 = 0U; /* EL0t */
    frame.sp_el0 = 0x7fff0000U;
    frame.esr_el1 = 0x54000000U; /* AArch64 SVC */
    frame.far_el1 = 0x12345000U;

    assert(frame.x[0] == 0x1000U);
    assert(frame.x[30] == 0x101eU);
    assert(frame.elr_el1 == 0x400000U);
    assert(frame.spsr_el1 == 0U);
    assert(frame.sp_el0 == 0x7fff0000U);
    assert(frame.esr_el1 == 0x54000000U);
    assert(frame.far_el1 == 0x12345000U);

    puts("User trap frame layout tests passed.");
    return 0;
}