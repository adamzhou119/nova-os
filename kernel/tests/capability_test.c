#include "capability.h"
#include "ipc.h"

#include <assert.h>

int main(void) {
    struct nova_cap_audit_record event;
    struct nova_ipc_message message;
    int64_t port, cap, updated;

    port = nova_ipc_port_create(10U);
    assert(port > 0);
    assert(!nova_capability_check(20U, 1U, NOVA_CAP_RESOURCE_IPC_PORT,
                                  (uint32_t)port, NOVA_CAP_RIGHT_SEND));
    assert(nova_capability_grant(20U, 30U, NOVA_CAP_RESOURCE_IPC_PORT,
                                 (uint32_t)port, NOVA_CAP_RIGHT_SEND) == -13);
    assert(nova_capability_grant(10U, 30U, NOVA_CAP_RESOURCE_IPC_PORT,
                                 (uint32_t)port, 4U) == -22);

    cap = nova_capability_grant(10U, 30U, NOVA_CAP_RESOURCE_IPC_PORT,
                                (uint32_t)port, NOVA_CAP_RIGHT_SEND);
    assert(cap > 0);
    assert(nova_capability_check(30U, (uint32_t)cap,
                                 NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                 NOVA_CAP_RIGHT_SEND));
    assert(!nova_capability_check(30U, (uint32_t)cap,
                                  NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                  NOVA_CAP_RIGHT_RECEIVE));
    assert(!nova_capability_check(31U, (uint32_t)cap,
                                  NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                  NOVA_CAP_RIGHT_SEND));
    assert(nova_ipc_send(30U, (uint32_t)port, "x", 1U) == 0);

    updated = nova_capability_grant(10U, 30U, NOVA_CAP_RESOURCE_IPC_PORT,
                                    (uint32_t)port, NOVA_CAP_RIGHT_RECEIVE);
    assert(updated == cap);
    assert(!nova_capability_check(30U, (uint32_t)cap,
                                  NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                  NOVA_CAP_RIGHT_SEND));
    assert(nova_capability_check(30U, (uint32_t)cap,
                                 NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                 NOVA_CAP_RIGHT_RECEIVE));
    assert(nova_ipc_send(30U, (uint32_t)port, "x", 1U) == -13);
    assert(nova_capability_revoke(99U, (uint32_t)cap) == -13);
    assert(nova_capability_revoke(10U, (uint32_t)cap) == 0);
    assert(!nova_capability_check(30U, (uint32_t)cap,
                                  NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                  NOVA_CAP_RIGHT_RECEIVE));
    assert(nova_ipc_receive(30U, (uint32_t)port,
                            &message, sizeof(message.payload)) == -13);
    assert(nova_capability_revoke(10U, (uint32_t)cap) == -2);

    cap = nova_capability_grant(10U, 40U, NOVA_CAP_RESOURCE_IPC_PORT,
                                (uint32_t)port, NOVA_CAP_RIGHT_SEND);
    assert(cap > 0);
    nova_capability_task_exit(40U);
    assert(!nova_capability_check(40U, (uint32_t)cap,
                                  NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
                                  NOVA_CAP_RIGHT_SEND));
    assert(nova_ipc_send(40U, (uint32_t)port, "x", 1U) == -13);

    assert(nova_capability_audit_read(1U, &event));
    assert(event.event == NOVA_CAP_AUDIT_DENIED);
    assert(event.actor == 20U);
    assert(event.resource_id == 0U);
    assert(nova_capability_audit_read(2U, &event));
    assert(event.event == NOVA_CAP_AUDIT_DENIED);
    assert(!nova_capability_audit_read(0U, &event));

    nova_capability_task_exit(10U);
    nova_ipc_close_owner(10U);
    assert(nova_ipc_send(30U, (uint32_t)port, "x", 1U) == -9);
    return 0;
}
