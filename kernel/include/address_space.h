#ifndef NOVA_ADDRESS_SPACE_H
#define NOVA_ADDRESS_SPACE_H

#include <stdint.h>

#define NOVA_ADDRESS_SPACE_MAX 16U
#define NOVA_ADDRESS_SPACE_PAGE_SIZE 4096U
/* TCR_EL1.AS is clear in this prototype, so ASIDs are 8-bit. */
#define NOVA_ADDRESS_SPACE_MAX_ASID 255U

struct nova_address_space_info {
    uint32_t id;
    uint16_t asid;
    uint16_t references;
    uint16_t active_cpus;
    uintptr_t root_physical;
};

/*
 * Address-space roots are owned by the registry after create succeeds. The
 * destroy hook is called exactly once when the final inactive reference is
 * released; a failed hook leaves the registry entry intact for retry.
 * Registry metadata is serialized with local IRQ masking on this single-core
 * prototype. The final destroy hook runs outside that critical section while
 * its slot is pinned against retain/activate/release. ASIDs are limited to
 * 1..255 until 16-bit ASID support is enabled and architecture-tested.
 */
typedef int (*nova_address_space_destroy_fn)(uintptr_t root_physical, uint16_t asid);
int nova_address_space_set_destroy_hook(nova_address_space_destroy_fn destroy);
int nova_address_space_create(uintptr_t root_physical, uint16_t asid,
                              uint32_t *address_space_id);
int nova_address_space_retain(uint32_t address_space_id);
int nova_address_space_release(uint32_t address_space_id);
int nova_address_space_activate(uint32_t address_space_id);
int nova_address_space_deactivate(uint32_t address_space_id);
int nova_address_space_get(uint32_t address_space_id,
                           struct nova_address_space_info *info);

#endif
