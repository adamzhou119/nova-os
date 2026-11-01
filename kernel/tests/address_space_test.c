#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "address_space.h"

static uintptr_t destroyed_root;
static int destroy_calls;
static int destroy_root(uintptr_t root) {
    ++destroy_calls;
    destroyed_root = root;
    return root != 0xDEAD000U;
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
    assert(!nova_address_space_release(second)); /* Failed teardown is retryable. */
    assert(nova_address_space_get(second, &info));
    assert(info.references == 1U && info.active_cpus == 0U);
    assert(destroy_calls == 3);
    /* Swap in a successful hook only after all live spaces are gone is forbidden. */

    puts("Address-space metadata host tests passed.");
    return 0;
}
