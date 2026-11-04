#ifndef NOVA_CAPABILITY_H
#define NOVA_CAPABILITY_H

#include <stdint.h>

#define NOVA_CAPABILITY_MAX_ENTRIES 32U
#define NOVA_CAPABILITY_RESOURCE_IPC_PORT 1U
#define NOVA_CAPABILITY_RIGHT_SEND    (1U << 0)
#define NOVA_CAPABILITY_RIGHT_RECEIVE (1U << 1)
#define NOVA_CAPABILITY_RIGHT_MASK \
    (NOVA_CAPABILITY_RIGHT_SEND | NOVA_CAPABILITY_RIGHT_RECEIVE)

/*
 * Kernel-local, deny-by-default capability registry.
 * Only IPC-port resources are supported in this first implementation.
 * Grants are issued by the resource owner, never by the holder/model.
 */
int nova_capability_grant(uint32_t issuer, uint32_t holder,
                          uint32_t resource_type, uint32_t resource_id,
                          uint32_t rights, uint32_t *handle);
int nova_capability_check(uint32_t holder, uint32_t resource_type,
                          uint32_t resource_id, uint32_t required_rights);
int nova_capability_revoke(uint32_t issuer, uint32_t handle);
/* Remove grants issued by or held by a task before its ID can be reused. */
void nova_capability_close_task(uint32_t task_id);

#endif
