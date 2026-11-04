#include "capability.h"
#include "ipc.h"

#include <stddef.h>

#define CAP_ERR_PERM (-13)
#define CAP_ERR_NOENT (-2)
#define CAP_ERR_INVAL (-22)
#define CAP_ERR_NOSPC (-28)
#define CAP_ERR_NOTSUP (-95)

struct capability_entry {
    uint32_t id;
    uint32_t issuer;
    uint32_t holder;
    uint32_t resource_id;
    uint32_t rights;
    uint16_t resource_type;
    uint8_t used;
};

static struct capability_entry entries[NOVA_CAP_MAX_ENTRIES];
static struct nova_cap_audit_record audit[NOVA_CAP_AUDIT_DEPTH];
static uint32_t next_id = 1U;
static uint32_t next_audit_sequence = 1U;

static void record_event(uint8_t event, uint32_t actor,
                         const struct capability_entry *entry,
                         uint32_t requested_rights) {
    struct nova_cap_audit_record *r;
    uint32_t sequence = next_audit_sequence++;
    if (sequence == 0U) {
        next_audit_sequence = 0U;
        return;
    }
    r = &audit[(sequence - 1U) % NOVA_CAP_AUDIT_DEPTH];
    r->sequence = sequence;
    r->actor = actor;
    r->holder = entry != 0 ? entry->holder : 0U;
    r->resource_id = entry != 0 ? entry->resource_id : 0U;
    r->capability_id = entry != 0 ? entry->id : 0U;
    r->rights = entry != 0 ? requested_rights : 0U;
    r->resource_type = entry != 0 ? entry->resource_type : 0U;
    r->event = event;
    r->reserved = 0U;
}

static struct capability_entry *find_id(uint32_t id) {
    size_t i;
    if (id == 0U) return 0;
    for (i = 0U; i < NOVA_CAP_MAX_ENTRIES; ++i)
        if (entries[i].used && entries[i].id == id) return &entries[i];
    return 0;
}

int64_t nova_capability_grant(uint32_t issuer, uint32_t holder,
                              uint32_t resource_type, uint32_t resource_id,
                              uint32_t rights) {
    size_t i;
    struct capability_entry *entry = 0;
    int is_update = 0;
    int ipc_result;
    if (issuer == 0U || holder == 0U || resource_id == 0U ||
        rights == 0U || (rights & ~NOVA_CAP_RIGHT_MASK) != 0U) {
        record_event(NOVA_CAP_AUDIT_DENIED, issuer, 0, rights);
        return CAP_ERR_INVAL;
    }
    if (resource_type != NOVA_CAP_RESOURCE_IPC_PORT) {
        record_event(NOVA_CAP_AUDIT_DENIED, issuer, 0, rights);
        return CAP_ERR_NOTSUP;
    }
    if (!nova_ipc_port_is_owner(issuer, resource_id)) {
        record_event(NOVA_CAP_AUDIT_DENIED, issuer, 0, rights);
        return CAP_ERR_PERM;
    }
    for (i = 0U; i < NOVA_CAP_MAX_ENTRIES; ++i) {
        if (entries[i].used && entries[i].issuer == issuer &&
            entries[i].holder == holder &&
            entries[i].resource_type == resource_type &&
            entries[i].resource_id == resource_id) {
            entry = &entries[i];
            is_update = 1;
            break;
        }
    }
    if (entry == 0) {
        if (next_id == 0U || next_id == UINT32_MAX) return CAP_ERR_NOSPC;
        for (i = 0U; i < NOVA_CAP_MAX_ENTRIES; ++i)
            if (!entries[i].used) break;
        if (i == NOVA_CAP_MAX_ENTRIES) return CAP_ERR_NOSPC;
        entry = &entries[i];
        entry->id = next_id++;
        entry->issuer = issuer;
        entry->holder = holder;
        entry->resource_type = (uint16_t)resource_type;
        entry->resource_id = resource_id;
        entry->used = 1U;
    }
    ipc_result = nova_ipc_port_grant(issuer, resource_id, holder, rights);
    if (ipc_result != 0) {
        if (!is_update) entry->used = 0U;
        record_event(NOVA_CAP_AUDIT_DENIED, issuer, entry, rights);
        return ipc_result;
    }
    entry->rights = rights;
    record_event(is_update ? NOVA_CAP_AUDIT_UPDATED : NOVA_CAP_AUDIT_GRANTED,
                 issuer, entry, rights);
    return (int64_t)entry->id;
}

int nova_capability_revoke(uint32_t issuer, uint32_t capability_id) {
    struct capability_entry *entry = find_id(capability_id);
    if (entry == 0) return CAP_ERR_NOENT;
    if (entry->issuer != issuer) {
        record_event(NOVA_CAP_AUDIT_DENIED, issuer, entry, 0U);
        return CAP_ERR_PERM;
    }
    if (entry->resource_type == NOVA_CAP_RESOURCE_IPC_PORT)
        (void)nova_ipc_port_revoke(issuer, entry->resource_id, entry->holder);
    record_event(NOVA_CAP_AUDIT_REVOKED, issuer, entry, entry->rights);
    entry->used = 0U;
    entry->id = 0U;
    entry->issuer = 0U;
    entry->holder = 0U;
    entry->resource_id = 0U;
    entry->rights = 0U;
    entry->resource_type = 0U;
    return 0;
}

int nova_capability_check(uint32_t holder, uint32_t capability_id,
                          uint32_t resource_type, uint32_t resource_id,
                          uint32_t required_rights) {
    struct capability_entry *entry = find_id(capability_id);
    if (entry == 0 || entry->holder != holder ||
        entry->resource_type != resource_type ||
        entry->resource_id != resource_id || required_rights == 0U ||
        (required_rights & ~NOVA_CAP_RIGHT_MASK) != 0U ||
        (entry->rights & required_rights) != required_rights)
        return 0;
    if (resource_type == NOVA_CAP_RESOURCE_IPC_PORT)
        return nova_ipc_port_is_owner(entry->issuer, resource_id) &&
               nova_ipc_port_has_rights(holder, resource_id, required_rights);
    return 0;
}

void nova_capability_task_exit(uint32_t task_id) {
    size_t i;
    if (task_id == 0U) return;
    for (i = 0U; i < NOVA_CAP_MAX_ENTRIES; ++i) {
        struct capability_entry *entry = &entries[i];
        if (!entry->used || (entry->issuer != task_id &&
                             entry->holder != task_id)) continue;
        if (entry->issuer == task_id && entry->resource_type == NOVA_CAP_RESOURCE_IPC_PORT) {
            /* The owner's port teardown follows this hook. */
        } else if (entry->resource_type == NOVA_CAP_RESOURCE_IPC_PORT) {
            (void)nova_ipc_port_revoke(entry->issuer, entry->resource_id,
                                       entry->holder);
        }
        record_event(NOVA_CAP_AUDIT_TASK_CLEANUP, task_id, entry, entry->rights);
        entry->used = 0U;
        entry->id = 0U;
        entry->issuer = 0U;
        entry->holder = 0U;
        entry->resource_id = 0U;
        entry->rights = 0U;
        entry->resource_type = 0U;
    }
}

int nova_capability_audit_read(uint32_t sequence,
                               struct nova_cap_audit_record *record) {
    const struct nova_cap_audit_record *stored;
    if (sequence == 0U || record == 0) return 0;
    stored = &audit[(sequence - 1U) % NOVA_CAP_AUDIT_DEPTH];
    if (stored->sequence != sequence) return 0;
    *record = *stored;
    return 1;
}
