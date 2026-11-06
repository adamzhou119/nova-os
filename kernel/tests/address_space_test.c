#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "address_space.h"

static uintptr_t destroyed_root;
static int destroy_calls;
static int allow_dead_root_destroy;
static uint32_t destroying_id;
static int destroy_pin_checks;
static int destroy_root(uintptr_t root, uint16_t asid) {
    struct nova_address_space_info during_destroy;
    (void)asid;
    ++destroy_calls;
    destroyed_root = root;
    if (root == 0xDEAD000U) {
        /* The final slot is pinned, but the hook must not run under IRQ lock. */
        destroy_pin_checks =
            !nova_address_space_get(destroying_id, &during_destroy) &&
            !nova_address_space_retain(destroying_id) &&
            !nova_address_space_activate(destroying_id) &&
            !nova_address_space_release(destroying_id);
        return allow_dead_root_destroy;
    }
    return 1;
}

int main(void) {
    struct nova_address_space_info info;
    uint32_t first;
    uint32_t second;

    assert(nova_address_space_set_destroy_hook(destroy_root));
    assert(!nova_address_space_create(0U, 1U, &first));
    assert(!nova_address_space_create(0x40001001U, 1U, &first));
    assert(!nova_address_space_create(0x40001000U, 0U, &first));
    assert(!nova_address_space_create(0x40001000U, 256U, &first));
    assert(!nova_address_space_create(0x40001000U, UINT16_MAX, &first));
    assert(!nova_address_space_create(0x40001000U, 1U, 0));

    assert(nova_address_space_create(0x40001000U, 1U, &first));
    assert(first != 0U);
    assert(!nova_address_space_create(0x40002000U, 1U, &second));
    assert(!nova_address_space_create(0x40001000U, 2U, &second)); /* A root has one registry owner. */
    assert(nova_address_space_get(first, &info));
    assert(info.id == first && info.asid == 1U);
    assert(info.references == 1U && info.active_cpus == 0U);
    assert(info.root_physical == 0x40001000U);

    assert(!nova_address_space_deactivate(first));
    assert(nova_address_space_activate(first));
    assert(!nova_address_space_activate(first));
    assert(!nova_address_space_release(first));
    assert(nova_address_space_retain(first));
    assert(nova_address_space_activate(first));
    assert(!nova_address_space_release(first));
    assert(nova_address_space_deactivate(first));
    assert(nova_address_space_release(first));
    assert(nova_address_space_get(first, &info));
    assert(info.references == 1U && info.active_cpus == 1U);
    assert(nova_address_space_deactivate(first));
    assert(nova_address_space_release(first));
    assert(destroy_calls == 1 && destroyed_root == 0x40001000U);
    assert(!nova_address_space_get(first, &info));
    assert(!nova_address_space_retain(first));

    assert(nova_address_space_create(0x40002000U, 1U, &second));
    assert(second != first); /* IDs are not reused after destruction. */
    assert(nova_address_space_release(second));
    assert(destroy_calls == 2 && destroyed_root == 0x40002000U);

    assert(nova_address_space_create(0xDEAD000U, 2U, &second));
    destroying_id = second;
    assert(!nova_address_space_release(second)); /* Failed teardown is retryable. */
    assert(destroy_pin_checks);
    assert(nova_address_space_get(second, &info));
    assert(info.references == 1U && info.active_cpus == 0U);
    assert(destroy_calls == 3);
    allow_dead_root_destroy = 1;
    assert(nova_address_space_release(second)); /* A later retry completes. */
    assert(destroy_calls == 4);
    assert(!nova_address_space_get(second, &info));

    puts("Address-space metadata host tests passed.");
    return 0;
}
