#include "service.h"

#include "ipc.h"

#include <assert.h>
#include <stdint.h>

int main(void) {
    int64_t port_a = nova_ipc_port_create(10U);
    int64_t port_b = nova_ipc_port_create(20U);
    size_t i;
    char name[NOVA_SERVICE_NAME_MAX + 2U];

    assert(port_a > 0);
    assert(port_b > 0);
    assert(nova_service_register(10U, "display.manager", (uint32_t)port_a) == 0);
    assert(nova_service_lookup("display.manager") == port_a);
    assert(nova_service_register(20U, "display.manager", (uint32_t)port_b) == -17);
    assert(nova_service_register(20U, "bad.owner", (uint32_t)port_a) == -13);
    assert(nova_service_unregister(20U, "display.manager") == -13);
    assert(nova_service_unregister(10U, "display.manager") == 0);
    assert(nova_service_lookup("display.manager") == -2);

    assert(nova_service_register(10U, "AI_orchestrator-v1",
                                 (uint32_t)port_a) == 0);
    assert(nova_service_lookup("AI_orchestrator-v1") == port_a);
    assert(nova_service_register(10U, "bad/name", (uint32_t)port_a) == -22);
    assert(nova_service_register(10U, "", (uint32_t)port_a) == -22);
    assert(nova_service_register(0U, "zero.owner", (uint32_t)port_a) == -22);
    assert(nova_service_register(10U, "zero.port", 0U) == -22);

    for (i = 0U; i < sizeof(name); ++i) name[i] = 'x';
    name[sizeof(name) - 1U] = '\0';
    assert(nova_service_register(10U, name, (uint32_t)port_a) == -22);

    nova_service_close_owner(10U);
    assert(nova_service_lookup("AI_orchestrator-v1") == -2);
    assert(nova_ipc_send(10U, (uint32_t)port_a, 0, 0U) == -9);

    for (i = 0U; i < NOVA_SERVICE_MAX_ENTRIES; ++i) {
        char slot_name[16] = "svc";
        slot_name[3] = (char)('a' + (i % 26U));
        slot_name[4] = (char)('A' + (i / 26U));
        slot_name[5] = '\0';
        assert(nova_service_register(20U, slot_name, (uint32_t)port_b) == 0);
    }
    assert(nova_service_register(20U, "overflow", (uint32_t)port_b) == -28);
    nova_service_close_owner(20U);
    assert(nova_service_lookup("svcaA") == -2);
    return 0;
}
