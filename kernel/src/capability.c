#include "capability.h"

#include "ipc.h"
#include "task.h"

#include <stddef.h>

#define NOVA_CAP_ERR_PERM  (-13)
#define NOVA_CAP_ERR_NOENT (-2)
#define NOVA_CAP_ERR_INVAL (-22)
#define NOVA_CAP_ERR_NOSPC (-28)
#define NOVA_CAP_ERR_NOTSUP (-95)
#define NOVA_CAP_ERR_EXIST (-17)

struct capability_entry {
    uint32_t handle;
    uint32_t issuer;
    uint32_t holder;
    uint32_t resource_type;
    uint32_t resource_id;
    uint32_t rights;
    uint8_t used;
};

static struct capability_entry entries[NOVA_CAPABILITY_MAX_ENTRIES];
static uint32_t next_handle = 1U;

static struct capability_entry *find_handle(uint32_t handle) {
    size_t i;
    if (handle == 0U) return 0;
    for (i = 0U; i < NOVA_CAPABILITY_MAX_ENTRIES; ++i)
        if (entries[i].used && entries[i].handle == handle)
            return &entries[i];
    return 0;
}

static int live_task(uint32_t id) {
    struct nova_task_info info;
    return id != 0U && nova_task_get(id, &info) &&
           info.state != NOVA_TASK_UNUSED && info.state != NOVA_TASK_ZOMBIE;
}

static uint32_t ipc_rights(uint32_t rights) {
    uint32_t mapped = 0U;
    if ((rights & NOVA_CAPABILITY_RIGHT_SEND) != 0U)
        mapped |= NOVA_IPC_RIGHT_SEND;
    if ((rights & NOVA_CAPABILITY_RIGHT_RECEIVE) != 0U)
        mapped |= NOVA_IPC_RIGHT_RECEIVE;
    return mapped;
}

static void clear_entry(struct capability_entry *entry) {
    entry->handle = 0U;
    entry->issuer = 0U;
    entry->holder = 0U;
    entry->resource_type = 0U;
    entry->resource_id = 0U;
    entry->rights = 0U;
    entry->used = 0U;
}

int nova_capability_grant(uint32_t issuer, uint32_t holder,
                          uint32_t resource_type, uint32_t resource_id,
                          uint32_t rights, uint32_t *handle) {
    size_t i;
    struct capability_entry *entry = 0;

    if (issuer == 0U || holder == 0U || issuer == holder ||
        resource_id == 0U || rights == 0U || handle == 0)
        return NOVA_CAP_ERR_INVAL;
    if (resource_type != NOVA_CAPABILITY_RESOURCE_IPC_PORT)
        return NOVA_CAP_ERR_NOTSUP;
    if ((rights & ~NOVA_CAPABILITY_RIGHT_MASK) != 0U)
        return NOVA_CAP_ERR_INVAL;
    /* The resource owner is the only authority allowed to issue this grant. */
    if (!nova_ipc_port_is_owner(issuer, resource_id))
        return NOVA_CAP_ERR_PERM;
    if (!live_task(holder)) return NOVA_CAP_ERR_NOENT;
    if (next_handle == 0U || next_handle == UINT32_MAX)
        return NOVA_CAP_ERR_NOSPC;

    for (i = 0U; i < NOVA_CAPABILITY_MAX_ENTRIES; ++i) {
        if (entries[i].used &&
            entries[i].holder == holder &&
            entries[i].resource_type == resource_type &&
            entries[i].resource_id == resource_id)
            return NOVA_CAP_ERR_EXIST;
        if (!entries[i].used && entry == 0)
            entry = &entries[i];
    }
    if (entry == 0) return NOVA_CAP_ERR_NOSPC;

    /*
     * Publish the broker record only after the IPC authority accepts the
     * matching rights. This keeps the capability table and enforcement table
     * from disagreeing when the underlying grant fails.
     */
    {
        int result = nova_ipc_port_grant(issuer, resource_id, holder,
                                         ipc_rights(rights));
        if (result != 0) return result;
    }

    entry->handle = next_handle++;
    entry->issuer = issuer;
    entry->holder = holder;
    entry->resource_type = resource_type;
    entry->resource_id = resource_id;
    entry->rights = rights;
    entry->used = 1U;
    *handle = entry->handle;
    return 0;
}

int nova_capability_check(uint32_t holder, uint32_t resource_type,
                          uint32_t resource_id, uint32_t required_rights) {
    size_t i;
    if (holder == 0U || resource_id == 0U || required_rights == 0U ||
        (required_rights & ~NOVA_CAPABILITY_RIGHT_MASK) != 0U)
        return NOVA_CAP_ERR_INVAL;
    if (resource_type != NOVA_CAPABILITY_RESOURCE_IPC_PORT)
        return NOVA_CAP_ERR_NOTSUP;
    if (!live_task(holder)) return NOVA_CAP_ERR_PERM;

    for (i = 0U; i < NOVA_CAPABILITY_MAX_ENTRIES; ++i) {
        const struct capability_entry *entry = &entries[i];
        if (!entry->used || entry->holder != holder ||
            entry->resource_type != resource_type ||
            entry->resource_id != resource_id)
            continue;
        /* Revalidate both resource ownership and effective IPC rights. */
        if (!nova_ipc_port_is_owner(entry->issuer, resource_id) ||
            !nova_ipc_port_has_rights(holder, resource_id,
                                      ipc_rights(required_rights)))
            return NOVA_CAP_ERR_PERM;
        if ((entry->rights & required_rights) == required_rights)
            return 0;
    }
    return NOVA_CAP_ERR_PERM;
}

int nova_capability_revoke(uint32_t issuer, uint32_t handle) {
    struct capability_entry *entry = find_handle(handle);
    int result;
    if (entry == 0) return NOVA_CAP_ERR_NOENT;
    if (issuer == 0U || entry->issuer != issuer ||
        !nova_ipc_port_is_owner(issuer, entry->resource_id))
        return NOVA_CAP_ERR_PERM;

    result = nova_ipc_port_revoke(issuer, entry->resource_id, entry->holder);
    /* A direct owner revocation may already have removed the IPC grant. */
    if (result != 0 && result != -9) return result;
    clear_entry(entry);
    return 0;
}

void nova_capability_close_task(uint32_t task_id) {
    size_t i;
    if (task_id == 0U) return;
    for (i = 0U; i < NOVA_CAPABILITY_MAX_ENTRIES; ++i) {
        struct capability_entry *entry = &entries[i];
        if (!entry->used ||
            (entry->issuer != task_id && entry->holder != task_id))
            continue;
        clear_entry(entry);
    }
}
