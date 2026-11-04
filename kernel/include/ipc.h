#ifndef NOVA_IPC_H
#define NOVA_IPC_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_IPC_MAX_PORTS 16U
#define NOVA_IPC_QUEUE_DEPTH 8U
#define NOVA_IPC_MAX_PAYLOAD 64U
#define NOVA_IPC_MAX_GRANTS 4U

#define NOVA_IPC_RIGHT_SEND    (1U << 0)
#define NOVA_IPC_RIGHT_RECEIVE (1U << 1)
#define NOVA_IPC_RIGHT_MASK (NOVA_IPC_RIGHT_SEND | NOVA_IPC_RIGHT_RECEIVE)

struct nova_ipc_message {
    uint32_t sender;
    uint16_t length;
    uint16_t reserved;
    uint8_t payload[NOVA_IPC_MAX_PAYLOAD];
};

/* Bounded, non-blocking, single-core IPC mailbox primitives. */
int64_t nova_ipc_port_create(uint32_t owner);
int nova_ipc_port_grant(uint32_t owner, uint32_t port,
                        uint32_t recipient, uint32_t rights);
int nova_ipc_port_revoke(uint32_t owner, uint32_t port, uint32_t recipient);
int nova_ipc_send(uint32_t sender, uint32_t port,
                  const void *payload, size_t length);
int64_t nova_ipc_receive(uint32_t receiver, uint32_t port,
                         struct nova_ipc_message *message,
                         size_t capacity);
int nova_ipc_port_close(uint32_t owner, uint32_t port);
void nova_ipc_close_owner(uint32_t owner);

#endif
