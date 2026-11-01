#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "mmu.h"
#include "user_memory.h"

static unsigned int access_checks;
static int expected_write;

int nova_mmu_verify_user_access(uintptr_t address, int write) {
    ++access_checks;
    (void)address;
    return write == expected_write;
}

int main(void) {
    uint8_t byte = 0U;
    static const uint8_t source = 0x5aU;

    /* Zero-length operations are harmless and do not inspect pointers. */
    assert(nova_user_copy_from(0, 0U, 0U) == 0);
    assert(nova_user_copy_to(0U, 0, 0U) == 0);
    assert(access_checks == 0U);

    /* Invalid and out-of-window ranges are rejected before dereference. */
    assert(nova_user_copy_from(&byte, NOVA_USER_VA_END, 1U) == -14);
    assert(nova_user_copy_from(&byte, NOVA_USER_VA_END - 1U, 2U) == -14);
    assert(nova_user_copy_to(NOVA_USER_VA_END, &source, 1U) == -14);
    assert(access_checks == 0U);

    /* Oversized requests fail deterministically without touching user VA. */
    assert(nova_user_copy_from(&byte, NOVA_USER_VA_BASE,
                               NOVA_USER_COPY_MAX + 1U) == -22);
    assert(nova_user_copy_to(NOVA_USER_VA_BASE, &source,
                             NOVA_USER_COPY_MAX + 1U) == -22);
    assert(access_checks == 0U);

    /* Kernel-side null buffers are rejected for non-empty copies. */
    assert(nova_user_copy_from(0, NOVA_USER_VA_BASE, 1U) == -14);
    assert(nova_user_copy_to(NOVA_USER_VA_BASE, 0, 1U) == -14);
    assert(access_checks == 0U);

    puts("user-memory bounded range tests passed");
    return 0;
}
