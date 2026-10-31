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
 * Metadata/lifetime registry only. The caller owns page-table allocation.
 * Mutations must be serialized; this prototype is single-core. ASIDs are limited
 * to 1..255 until 16-bit ASID support is enabled and architecture-tested.
 */
int nova_address_space_create(uintptr_t root_physical, uint16_t asid,
                              uint32_t *address_space_id);
int nova_address_space_retain(uint32_t address_space_id);
int nova_address_space_release(uint32_t address_space_id);
int nova_address_space_activate(uint32_t address_space_id);
int nova_address_space_deactivate(uint32_t address_space_id);
int nova_address_space_get(uint32_t address_space_id,
                           struct nova_address_space_info *info);

#endif
