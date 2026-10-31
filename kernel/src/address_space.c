#include "address_space.h"

#include <stddef.h>

struct address_space_slot {
    struct nova_address_space_info info;
    int used;
};

static struct address_space_slot spaces[NOVA_ADDRESS_SPACE_MAX];
static uint32_t next_address_space_id = 1U;

static struct address_space_slot *find_space(uint32_t id) {
    size_t i;
    if (id == 0U) return 0; /* ID zero is the immortal kernel address space. */
    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i)
        if (spaces[i].used && spaces[i].info.id == id) return &spaces[i];
    return 0;
}

int nova_address_space_create(uintptr_t root_physical, uint16_t asid,
                              uint32_t *address_space_id) {
    size_t i;
    if (root_physical == 0U ||
        (root_physical & (NOVA_ADDRESS_SPACE_PAGE_SIZE - 1U)) != 0U ||
        asid == 0U || address_space_id == 0 || next_address_space_id == 0U)
        return 0;

    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i) {
        if (spaces[i].used && spaces[i].info.asid == asid) return 0;
    }
    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i)
        if (!spaces[i].used) break;
    if (i == NOVA_ADDRESS_SPACE_MAX) return 0;

    spaces[i].info.id = next_address_space_id++;
    spaces[i].info.asid = asid;
    spaces[i].info.references = 1U;
    spaces[i].info.active_cpus = 0U;
    spaces[i].info.root_physical = root_physical;
    spaces[i].used = 1;
    *address_space_id = spaces[i].info.id;
    return 1;
}

int nova_address_space_retain(uint32_t address_space_id) {
    struct address_space_slot *space = find_space(address_space_id);
    if (space == 0 || space->info.references == UINT16_MAX) return 0;
    ++space->info.references;
    return 1;
}

int nova_address_space_release(uint32_t address_space_id) {
    struct address_space_slot *space = find_space(address_space_id);
    if (space == 0 || space->info.references == 0U) return 0;
    if (space->info.references == 1U && space->info.active_cpus != 0U)
        return 0;
    --space->info.references;
    if (space->info.references == 0U) {
        space->used = 0;
        space->info.id = 0U;
        space->info.asid = 0U;
        space->info.active_cpus = 0U;
        space->info.root_physical = 0U;
    }
    return 1;
}

int nova_address_space_activate(uint32_t address_space_id) {
    struct address_space_slot *space = find_space(address_space_id);
    if (space == 0 || space->info.active_cpus == UINT16_MAX ||
        space->info.active_cpus >= space->info.references)
        return 0;
    ++space->info.active_cpus;
    return 1;
}

int nova_address_space_deactivate(uint32_t address_space_id) {
    struct address_space_slot *space = find_space(address_space_id);
    if (space == 0 || space->info.active_cpus == 0U) return 0;
    --space->info.active_cpus;
    return 1;
}

int nova_address_space_get(uint32_t address_space_id,
                           struct nova_address_space_info *info) {
    struct address_space_slot *space = find_space(address_space_id);
    if (space == 0 || info == 0) return 0;
    *info = space->info;
    return 1;
}
