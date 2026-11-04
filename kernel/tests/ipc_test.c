#include "ipc.h"

#include <assert.h>
#include <string.h>

int main(void) {
    const char hello[] = "hello service";
    struct nova_ipc_message message;
    int64_t port = nova_ipc_port_create(1U);
    size_t i;

    assert(port > 0);
    assert(nova_ipc_port_grant(1U, (uint32_t)port, 2U,
                               NOVA_IPC_RIGHT_SEND) == 0);
    assert(nova_ipc_port_grant(1U, (uint32_t)port, 3U,
                               NOVA_IPC_RIGHT_RECEIVE) == 0);
    assert(nova_ipc_send(2U, (uint32_t)port, hello, sizeof(hello)) == 0);
    assert(nova_ipc_receive(3U, (uint32_t)port, &message,
                            sizeof(message.payload)) == (int64_t)sizeof(hello));
    /* Receive-only clients cannot inject messages; send-only cannot consume. */
    assert(nova_ipc_send(3U, (uint32_t)port, hello, sizeof(hello)) == -13);
    assert(nova_ipc_receive(2U, (uint32_t)port, &message,
                            sizeof(message.payload)) == -13);
    assert(nova_ipc_send(9U, (uint32_t)port, hello, sizeof(hello)) == -13);

    assert(nova_ipc_send(2U, (uint32_t)port, hello, sizeof(hello)) == 0);
    assert(nova_ipc_receive(3U, (uint32_t)port, &message, 2U) == -90);
    assert(nova_ipc_receive(3U, (uint32_t)port, &message,
                            sizeof(message.payload)) == (int64_t)sizeof(hello));
    assert(message.sender == 2U);
    assert(memcmp(message.payload, hello, sizeof(hello)) == 0);
    assert(nova_ipc_receive(3U, (uint32_t)port, &message,
                            sizeof(message.payload)) == -11);

    for (i = 0U; i < NOVA_IPC_QUEUE_DEPTH; ++i)
        assert(nova_ipc_send(2U, (uint32_t)port, hello, sizeof(hello)) == 0);
    assert(nova_ipc_send(2U, (uint32_t)port, hello, sizeof(hello)) == -11);
    for (i = 0U; i < NOVA_IPC_QUEUE_DEPTH; ++i)
        assert(nova_ipc_receive(1U, (uint32_t)port, &message,
                                sizeof(message.payload)) == (int64_t)sizeof(hello));

    assert(nova_ipc_port_revoke(1U, (uint32_t)port, 2U) == 0);
    assert(nova_ipc_send(2U, (uint32_t)port, hello, sizeof(hello)) == -13);
    assert(nova_ipc_port_grant(2U, (uint32_t)port, 4U,
                               NOVA_IPC_RIGHT_SEND) == -13);
    assert(nova_ipc_port_close(2U, (uint32_t)port) == -13);
    assert(nova_ipc_port_close(1U, (uint32_t)port) == 0);
    assert(nova_ipc_send(2U, (uint32_t)port, hello, sizeof(hello)) == -9);

    port = nova_ipc_port_create(5U);
    assert(port > 0);
    assert(nova_ipc_port_grant(5U, (uint32_t)port, 6U,
                               NOVA_IPC_RIGHT_SEND) == 0);
    nova_ipc_close_owner(5U);
    assert(nova_ipc_send(6U, (uint32_t)port, hello, sizeof(hello)) == -9);
    assert(nova_ipc_port_create(0U) == -22);
    port = nova_ipc_port_create(7U);
    assert(port > 0);
    assert(nova_ipc_send(7U, (uint32_t)port, hello,
                         NOVA_IPC_MAX_PAYLOAD + 1U) == -22);
    assert(nova_ipc_send(1U, 99U, hello, sizeof(hello)) == -9);
    return 0;
}
