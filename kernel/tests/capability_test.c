#include "capability.h"
#include "ipc.h"
#include "task.h"

#include <assert.h>
#include <string.h>

static uint32_t live_ids[] = { 1U, 2U, 3U };
int nova_task_get(uint32_t id, struct nova_task_info *info) {
    size_t i;
    for (i = 0U; i < sizeof(live_ids) / sizeof(live_ids[0]); ++i) {
        if (live_ids[i] == id) {
            if (info != 0) {
                memset(info, 0, sizeof(*info));
                info->id = id;
                info->state = NOVA_TASK_READY;
            }
            return 1;
        }
    }
    return 0;
}
int nova_ipc_port_is_owner(uint32_t owner, uint32_t port) {
    return owner == 1U && port == 77U;
}

int main(void) {
    uint32_t handle = 0U, stale = 0U;
    assert(nova_capability_check(2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND) == -13);
    assert(nova_capability_grant(2U, 3U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND, &handle) == -13);
    assert(nova_capability_grant(1U, 99U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND, &handle) == -2);
    assert(nova_capability_grant(1U, 2U, 99U, 77U,
                                 NOVA_CAPABILITY_RIGHT_SEND, &handle) == -95);
    assert(nova_capability_grant(1U, 2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, 4U, &handle) == -22);
    assert(nova_capability_grant(1U, 2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND, &handle) == 0);
    assert(handle != 0U);
    assert(nova_capability_check(2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND) == 0);
    assert(nova_capability_check(2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_RECEIVE) == -13);
    assert(nova_capability_check(3U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND) == -13);
    assert(nova_capability_revoke(2U, handle) == -13);
    assert(nova_capability_revoke(1U, handle) == 0);
    assert(nova_capability_check(2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND) == -13);
    assert(nova_capability_revoke(1U, handle) == -2);

    assert(nova_capability_grant(1U, 2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND, &stale) == 0);
    nova_capability_close_task(2U);
    assert(nova_capability_check(2U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_SEND) == -13);
    assert(nova_capability_revoke(1U, stale) == -2);

    /* Handles are monotonic: a freed slot never revives an old handle. */
    assert(nova_capability_grant(1U, 3U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_RECEIVE, &handle) == 0);
    assert(handle > stale);
    assert(nova_capability_check(3U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_RECEIVE) == 0);
    nova_capability_close_task(1U);
    assert(nova_capability_check(3U, NOVA_CAPABILITY_RESOURCE_IPC_PORT,
                                 77U, NOVA_CAPABILITY_RIGHT_RECEIVE) == -13);
    return 0;
}
