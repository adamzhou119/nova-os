#include "address_space.h"

#include <stddef.h>
#include "interrupt.h"

struct address_space_slot {
    struct nova_address_space_info info;
    int used;
    int destroying;
};

static struct address_space_slot spaces[NOVA_ADDRESS_SPACE_MAX];
static uint32_t next_address_space_id = 1U;
static nova_address_space_destroy_fn destroy_hook;

/* This prototype is single-core: local IRQ masking serializes registry access. */
static uintptr_t address_space_lock(void) {
#if defined(__aarch64__)
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2"
                     : "=r"(daif) :: "memory");
    return daif;
#else
    return 0U;
#endif
}

static void address_space_unlock(uintptr_t daif) {
#if defined(__aarch64__)
    __asm__ volatile("msr daif, %0" :: "r"(daif) : "memory");
#else
    (void)daif;
#endif
}

static struct address_space_slot *find_space_locked(uint32_t id) {
    size_t i;
    if (id == 0U) return 0;
    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i)
        if (spaces[i].used && spaces[i].info.id == id) return &spaces[i];
    return 0;
}

int nova_address_space_set_destroy_hook(nova_address_space_destroy_fn destroy) {
    uintptr_t saved_daif;
    size_t i;
    int result = 1;
    if (destroy == 0) return 0;
    saved_daif = address_space_lock();
    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i) {
        if (spaces[i].used) {
            result = 0;
            break;
        }
    }
    if (result) destroy_hook = destroy;
    address_space_unlock(saved_daif);
    return result;
}

int nova_address_space_create(uintptr_t root_physical, uint16_t asid,
                              uint32_t *address_space_id) {
    uintptr_t saved_daif;
    size_t i;
    int result = 0;
    if (root_physical == 0U ||
        (root_physical & (NOVA_ADDRESS_SPACE_PAGE_SIZE - 1U)) != 0U ||
        asid == 0U || asid > NOVA_ADDRESS_SPACE_MAX_ASID ||
        address_space_id == 0)
        return 0;

    saved_daif = address_space_lock();
    if (next_address_space_id == 0U) goto out;
    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i) {
        if (spaces[i].used &&
            (spaces[i].info.asid == asid ||
             spaces[i].info.root_physical == root_physical))
            goto out;
    }
    for (i = 0; i < NOVA_ADDRESS_SPACE_MAX; ++i)
        if (!spaces[i].used) break;
    if (i == NOVA_ADDRESS_SPACE_MAX) goto out;

    spaces[i].info.id = next_address_space_id++;
    spaces[i].info.asid = asid;
    spaces[i].info.references = 1U;
    spaces[i].info.active_cpus = 0U;
    spaces[i].info.root_physical = root_physical;
    spaces[i].destroying = 0;
    spaces[i].used = 1;
    *address_space_id = spaces[i].info.id;
    result = 1;
out:
    address_space_unlock(saved_daif);
    return result;
}

int nova_address_space_retain(uint32_t address_space_id) {
    uintptr_t saved_daif = address_space_lock();
    struct address_space_slot *space = find_space_locked(address_space_id);
    int result = 0;
    if (space != 0 && !space->destroying &&
        space->info.references != UINT16_MAX) {
        ++space->info.references;
        result = 1;
    }
    address_space_unlock(saved_daif);
    return result;
}

int nova_address_space_release(uint32_t address_space_id) {
    uintptr_t saved_daif = address_space_lock();
    struct address_space_slot *space = find_space_locked(address_space_id);
    nova_address_space_destroy_fn destroy;
    uintptr_t root;
    uint16_t asid;
    int result;

    if (space == 0 || space->destroying ||
        space->info.references == 0U ||
        space->info.active_cpus >= space->info.references) {
        address_space_unlock(saved_daif);
        return 0;
    }

    /* Non-final releases are bounded metadata updates. */
    if (space->info.references > 1U) {
        --space->info.references;
        address_space_unlock(saved_daif);
        return 1;
    }

    /*
     * Pin the final slot against retain/create/release while the potentially
     * expensive page-table destructor runs with IRQs enabled.
     */
    if (destroy_hook == 0) {
        address_space_unlock(saved_daif);
        return 0;
    }
    space->destroying = 1;
    destroy = destroy_hook;
    root = space->info.root_physical;
    asid = space->info.asid;
    address_space_unlock(saved_daif);

    result = destroy(root, asid);

    saved_daif = address_space_lock();
    space = find_space_locked(address_space_id);
    if (space == 0 || !space->destroying ||
        space->info.references != 1U || space->info.active_cpus != 0U) {
        /* Registry invariants were violated while the slot was pinned. */
        address_space_unlock(saved_daif);
        return 0;
    }
    if (!result) {
        space->destroying = 0;
        address_space_unlock(saved_daif);
        return 0;
    }

    space->used = 0;
    space->destroying = 0;
    space->info.id = 0U;
    space->info.asid = 0U;
    space->info.references = 0U;
    space->info.active_cpus = 0U;
    space->info.root_physical = 0U;
    address_space_unlock(saved_daif);
    return 1;
}

int nova_address_space_activate(uint32_t address_space_id) {
    uintptr_t saved_daif = address_space_lock();
    struct address_space_slot *space = find_space_locked(address_space_id);
    int result = 0;
    if (space != 0 && !space->destroying &&
        space->info.active_cpus != UINT16_MAX &&
        space->info.active_cpus < space->info.references) {
        ++space->info.active_cpus;
        result = 1;
    }
    address_space_unlock(saved_daif);
    return result;
}

int nova_address_space_deactivate(uint32_t address_space_id) {
    uintptr_t saved_daif = address_space_lock();
    struct address_space_slot *space = find_space_locked(address_space_id);
    int result = 0;
    if (space != 0 && space->info.active_cpus != 0U) {
        --space->info.active_cpus;
        result = 1;
    }
    address_space_unlock(saved_daif);
    return result;
}

int nova_address_space_get(uint32_t address_space_id,
                           struct nova_address_space_info *info) {
    uintptr_t saved_daif;
    struct address_space_slot *space;
    int result = 0;
    if (info == 0) return 0;
    saved_daif = address_space_lock();
    space = find_space_locked(address_space_id);
    if (space != 0 && !space->destroying) {
        *info = space->info;
        result = 1;
    }
    address_space_unlock(saved_daif);
    return result;
}
