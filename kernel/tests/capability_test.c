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

    /*
     * Revocation blocks future sends but does not silently discard messages
     * already accepted into the mailbox; the owner can still drain them.
     */
    {
        int64_t queued_port = nova_ipc_port_create(11U);
        int64_t sender_cap;
        assert(queued_port > 0);
        sender_cap = nova_capability_grant(11U, 33U,
                                           NOVA_CAP_RESOURCE_IPC_PORT,
                                           (uint32_t)queued_port,
                                           NOVA_CAP_RIGHT_SEND);
        assert(sender_cap > 0);
        assert(nova_ipc_send(33U, (uint32_t)queued_port, "q", 2U) == 0);
        assert(nova_capability_revoke(11U, (uint32_t)sender_cap) == 0);
        assert(!nova_capability_check(33U, (uint32_t)sender_cap,
                                      NOVA_CAP_RESOURCE_IPC_PORT,
                                      (uint32_t)queued_port,
                                      NOVA_CAP_RIGHT_SEND));
        assert(nova_ipc_send(33U, (uint32_t)queued_port, "x", 2U) == -13);
        assert(nova_ipc_receive(11U, (uint32_t)queued_port,
                                &message, sizeof(message.payload)) == 2);
        assert(message.sender == 33U && message.payload[0] == 'q');
        assert(nova_ipc_port_close(11U, (uint32_t)queued_port) == 0);
    }

    /* Revoking RECEIVE blocks the holder while preserving the owner's queue. */
    {
        int64_t queued_port = nova_ipc_port_create(12U);
        int64_t receiver_cap;
        assert(queued_port > 0);
        assert(nova_ipc_send(12U, (uint32_t)queued_port, "r", 2U) == 0);
        receiver_cap = nova_capability_grant(12U, 34U,
                                             NOVA_CAP_RESOURCE_IPC_PORT,
                                             (uint32_t)queued_port,
                                             NOVA_CAP_RIGHT_RECEIVE);
        assert(receiver_cap > 0);
        assert(nova_capability_revoke(12U, (uint32_t)receiver_cap) == 0);
        assert(nova_ipc_receive(34U, (uint32_t)queued_port,
                                &message, sizeof(message.payload)) == -13);
        assert(nova_ipc_receive(12U, (uint32_t)queued_port,
                                &message, sizeof(message.payload)) == 2);
        assert(message.sender == 12U && message.payload[0] == 'r');
        assert(nova_ipc_port_close(12U, (uint32_t)queued_port) == 0);
    }

    /* A destroyed endpoint makes its old capability safely retireable. */
    {
        int64_t stale_port = nova_ipc_port_create(10U);
        int64_t stale_cap;
        assert(stale_port > 0);
        stale_cap = nova_capability_grant(10U, 32U,
                                          NOVA_CAP_RESOURCE_IPC_PORT,
                                          (uint32_t)stale_port,
                                          NOVA_CAP_RIGHT_SEND);
        assert(stale_cap > 0);
        assert(nova_ipc_port_close(10U, (uint32_t)stale_port) == 0);
        assert(nova_capability_revoke(10U, (uint32_t)stale_cap) == 0);
        assert(nova_capability_revoke(10U, (uint32_t)stale_cap) == -2);
    }

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

    /* Issuer exit invalidates every credential it minted. */
    {
        int64_t issuer_exit_cap = nova_capability_grant(
            10U, 41U, NOVA_CAP_RESOURCE_IPC_PORT, (uint32_t)port,
            NOVA_CAP_RIGHT_SEND);
        assert(issuer_exit_cap > 0);
        assert(nova_capability_check(41U, (uint32_t)issuer_exit_cap,
                                     NOVA_CAP_RESOURCE_IPC_PORT,
                                     (uint32_t)port, NOVA_CAP_RIGHT_SEND));
        nova_capability_task_exit(10U);
        assert(!nova_capability_check(41U, (uint32_t)issuer_exit_cap,
                                      NOVA_CAP_RESOURCE_IPC_PORT,
                                      (uint32_t)port, NOVA_CAP_RIGHT_SEND));
    }
    nova_ipc_close_owner(10U);
    assert(nova_ipc_send(30U, (uint32_t)port, "x", 1U) == -9);
    return 0;
}
