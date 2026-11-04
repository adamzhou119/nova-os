#ifndef NOVA_CAPABILITY_H
#define NOVA_CAPABILITY_H

#include <stdint.h>

#define NOVA_CAP_MAX_ENTRIES 32U
#define NOVA_CAP_AUDIT_DEPTH 32U
#define NOVA_CAP_RESOURCE_IPC_PORT 1U
#define NOVA_CAP_RIGHT_SEND (1U << 0)
#define NOVA_CAP_RIGHT_RECEIVE (1U << 1)
#define NOVA_CAP_RIGHT_MASK (NOVA_CAP_RIGHT_SEND | NOVA_CAP_RIGHT_RECEIVE)

enum nova_cap_audit_event {
    NOVA_CAP_AUDIT_GRANTED = 1,
    NOVA_CAP_AUDIT_UPDATED,
    NOVA_CAP_AUDIT_REVOKED,
    NOVA_CAP_AUDIT_DENIED,
    NOVA_CAP_AUDIT_TASK_CLEANUP
};

struct nova_cap_audit_record {
    uint32_t sequence;
    uint32_t actor;
    uint32_t holder;
    uint32_t resource_id;
    uint32_t capability_id;
    uint32_t rights;
    uint16_t resource_type;
    uint8_t event;
    uint8_t reserved;
};

/* Trusted kernel/system-service API. Untrusted EL0 tasks cannot issue grants. */
int64_t nova_capability_grant(uint32_t issuer, uint32_t holder,
                              uint32_t resource_type, uint32_t resource_id,
                              uint32_t rights);
int nova_capability_revoke(uint32_t issuer, uint32_t capability_id);
int nova_capability_check(uint32_t holder, uint32_t capability_id,
                          uint32_t resource_type, uint32_t resource_id,
                          uint32_t required_rights);
void nova_capability_task_exit(uint32_t task_id);
int nova_capability_audit_read(uint32_t sequence,
                               struct nova_cap_audit_record *record);

#endif
