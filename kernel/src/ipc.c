#include "ipc.h"

#define NOVA_IPC_ERR_PERM  (-13)
#define NOVA_IPC_ERR_AGAIN (-11)
#define NOVA_IPC_ERR_BADF  (-9)
#define NOVA_IPC_ERR_INVAL (-22)
#define NOVA_IPC_ERR_NOSPC (-28)
#define NOVA_IPC_ERR_MSGSIZE (-90)

struct ipc_grant {
    uint32_t task;
    uint32_t rights;
};

struct ipc_port {
    uint32_t id;
    uint32_t owner;
    uint8_t head;
    uint8_t count;
    uint8_t grant_count;
    uint8_t used;
    struct ipc_grant grants[NOVA_IPC_MAX_GRANTS];
    struct nova_ipc_message queue[NOVA_IPC_QUEUE_DEPTH];
};

static struct ipc_port ports[NOVA_IPC_MAX_PORTS];
static uint32_t next_port_id = 1U;

static struct ipc_port *find_port(uint32_t id) {
    size_t i;
    if (id == 0U) return 0;
    for (i = 0U; i < NOVA_IPC_MAX_PORTS; ++i)
        if (ports[i].used && ports[i].id == id) return &ports[i];
    return 0;
}

static uint32_t rights_for(const struct ipc_port *port, uint32_t task) {
    size_t i;
    if (task == port->owner) return NOVA_IPC_RIGHT_MASK;
    for (i = 0U; i < port->grant_count; ++i)
        if (port->grants[i].task == task) return port->grants[i].rights;
    return 0U;
}

static void clear_port(struct ipc_port *port) {
    size_t i, j;
    for (i = 0U; i < NOVA_IPC_QUEUE_DEPTH; ++i) {
        port->queue[i].sender = 0U;
        port->queue[i].length = 0U;
        port->queue[i].reserved = 0U;
        for (j = 0U; j < NOVA_IPC_MAX_PAYLOAD; ++j)
            port->queue[i].payload[j] = 0U;
    }
    for (i = 0U; i < NOVA_IPC_MAX_GRANTS; ++i) {
        port->grants[i].task = 0U;
        port->grants[i].rights = 0U;
    }
    port->id = 0U;
    port->owner = 0U;
    port->head = 0U;
    port->count = 0U;
    port->grant_count = 0U;
    port->used = 0U;
}

int64_t nova_ipc_port_create(uint32_t owner) {
    size_t i;
    uint32_t id;
    if (owner == 0U) return NOVA_IPC_ERR_INVAL;
    if (next_port_id == 0U || next_port_id == UINT32_MAX)
        return NOVA_IPC_ERR_NOSPC;
    for (i = 0U; i < NOVA_IPC_MAX_PORTS; ++i)
        if (!ports[i].used) break;
    if (i == NOVA_IPC_MAX_PORTS) return NOVA_IPC_ERR_NOSPC;
    id = next_port_id++;
    clear_port(&ports[i]);
    ports[i].id = id;
    ports[i].owner = owner;
    ports[i].used = 1U;
    return (int64_t)id;
}

int nova_ipc_port_is_owner(uint32_t owner, uint32_t id) {
    struct ipc_port *port = find_port(id);
    return port != 0 && owner != 0U && port->owner == owner;
}

int nova_ipc_port_has_rights(uint32_t task, uint32_t id, uint32_t rights) {
    struct ipc_port *port = find_port(id);
    if (port == 0 || task == 0U || rights == 0U ||
        (rights & ~NOVA_IPC_RIGHT_MASK) != 0U)
        return 0;
    return (rights_for(port, task) & rights) == rights;
}

int nova_ipc_port_grant(uint32_t owner, uint32_t id,
                        uint32_t recipient, uint32_t rights) {
    struct ipc_port *port = find_port(id);
    size_t i;
    if (port == 0) return NOVA_IPC_ERR_BADF;
    if (owner != port->owner) return NOVA_IPC_ERR_PERM;
    if (recipient == 0U || recipient == owner || rights == 0U ||
        (rights & ~NOVA_IPC_RIGHT_MASK) != 0U) return NOVA_IPC_ERR_INVAL;
    for (i = 0U; i < port->grant_count; ++i) {
        if (port->grants[i].task == recipient) {
            port->grants[i].rights = rights;
            return 0;
        }
    }
    if (port->grant_count == NOVA_IPC_MAX_GRANTS) return NOVA_IPC_ERR_NOSPC;
    port->grants[port->grant_count].task = recipient;
    port->grants[port->grant_count].rights = rights;
    ++port->grant_count;
    return 0;
}

int nova_ipc_port_revoke(uint32_t owner, uint32_t id, uint32_t recipient) {
    struct ipc_port *port = find_port(id);
    size_t i, j;
    if (port == 0) return NOVA_IPC_ERR_BADF;
    if (owner != port->owner) return NOVA_IPC_ERR_PERM;
    for (i = 0U; i < port->grant_count; ++i) {
        if (port->grants[i].task != recipient) continue;
        for (j = i + 1U; j < port->grant_count; ++j)
            port->grants[j - 1U] = port->grants[j];
        --port->grant_count;
        port->grants[port->grant_count].task = 0U;
        port->grants[port->grant_count].rights = 0U;
        return 0;
    }
    return NOVA_IPC_ERR_BADF;
}

int nova_ipc_send(uint32_t sender, uint32_t id,
                  const void *payload, size_t length) {
    struct ipc_port *port = find_port(id);
    size_t tail, i;
    struct nova_ipc_message *message;
    if (port == 0) return NOVA_IPC_ERR_BADF;
    if ((rights_for(port, sender) & NOVA_IPC_RIGHT_SEND) == 0U)
        return NOVA_IPC_ERR_PERM;
    if (length > NOVA_IPC_MAX_PAYLOAD || (payload == 0 && length != 0U))
        return NOVA_IPC_ERR_INVAL;
    if (port->count == NOVA_IPC_QUEUE_DEPTH) return NOVA_IPC_ERR_AGAIN;
    tail = ((size_t)port->head + port->count) % NOVA_IPC_QUEUE_DEPTH;
    message = &port->queue[tail];
    message->sender = sender;
    message->length = (uint16_t)length;
    message->reserved = 0U;
    for (i = 0U; i < length; ++i)
        message->payload[i] = ((const uint8_t *)payload)[i];
    for (; i < NOVA_IPC_MAX_PAYLOAD; ++i) message->payload[i] = 0U;
    ++port->count;
    return 0;
}

int64_t nova_ipc_receive(uint32_t receiver, uint32_t id,
                         struct nova_ipc_message *message,
                         size_t capacity) {
    struct ipc_port *port = find_port(id);
    struct nova_ipc_message *queued;
    size_t i;
    if (port == 0) return NOVA_IPC_ERR_BADF;
    if ((rights_for(port, receiver) & NOVA_IPC_RIGHT_RECEIVE) == 0U)
        return NOVA_IPC_ERR_PERM;
    if (message == 0) return NOVA_IPC_ERR_INVAL;
    if (port->count == 0U) return NOVA_IPC_ERR_AGAIN;
    queued = &port->queue[port->head];
    if (capacity < queued->length) return NOVA_IPC_ERR_MSGSIZE;
    message->sender = queued->sender;
    message->length = queued->length;
    message->reserved = 0U;
    for (i = 0U; i < queued->length; ++i)
        message->payload[i] = queued->payload[i];
    for (; i < NOVA_IPC_MAX_PAYLOAD; ++i) message->payload[i] = 0U;
    queued->sender = 0U;
    queued->length = 0U;
    for (i = 0U; i < NOVA_IPC_MAX_PAYLOAD; ++i) queued->payload[i] = 0U;
    port->head = (uint8_t)(((size_t)port->head + 1U) % NOVA_IPC_QUEUE_DEPTH);
    --port->count;
    return (int64_t)message->length;
}

int nova_ipc_port_close(uint32_t owner, uint32_t id) {
    struct ipc_port *port = find_port(id);
    if (port == 0) return NOVA_IPC_ERR_BADF;
    if (owner != port->owner) return NOVA_IPC_ERR_PERM;
    clear_port(port);
    return 0;
}

void nova_ipc_close_owner(uint32_t owner) {
    size_t i, j;
    if (owner == 0U) return;
    for (i = 0U; i < NOVA_IPC_MAX_PORTS; ++i) {
        struct ipc_port *port = &ports[i];
        if (!port->used) continue;
        if (port->owner == owner) {
            clear_port(port);
            continue;
        }
        /* Remove grants held by the exiting task before its ID can be reused. */
        for (j = 0U; j < port->grant_count; ) {
            size_t k;
            if (port->grants[j].task != owner) {
                ++j;
                continue;
            }
            for (k = j + 1U; k < port->grant_count; ++k)
                port->grants[k - 1U] = port->grants[k];
            --port->grant_count;
            port->grants[port->grant_count].task = 0U;
            port->grants[port->grant_count].rights = 0U;
        }
    }
}
