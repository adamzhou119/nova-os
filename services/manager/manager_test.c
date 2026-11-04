#include "manager.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    struct nova_service_manager manager;
    struct nova_managed_service service;

    nova_service_manager_init(&manager);
    assert(!nova_service_manager_register(0, 1U));
    assert(!nova_service_manager_register(&manager, 0U));
    assert(nova_service_manager_register(&manager, 1U));
    assert(!nova_service_manager_register(&manager, 1U));

    /* Only the task assigned to a start attempt may publish an endpoint. */
    assert(!nova_service_manager_publish(&manager, 1U, 10U, 20U));
    assert(!nova_service_manager_begin_start(&manager, 1U, 0U));
    assert(nova_service_manager_begin_start(&manager, 1U, 10U));
    assert(!nova_service_manager_publish(&manager, 1U, 11U, 20U));
    assert(!nova_service_manager_begin_stop(&manager, 1U, 10U));
    assert(nova_service_manager_publish(&manager, 1U, 10U, 20U));
    assert(nova_service_manager_get(&manager, 1U, &service));
    assert(service.state == NOVA_MANAGED_RUNNING);
    assert(service.task_id == 10U && service.endpoint == 20U);

    /* A stale task cannot stop a replacement service instance. */
    assert(!nova_service_manager_begin_stop(&manager, 1U, 11U));
    assert(nova_service_manager_begin_stop(&manager, 1U, 10U));
    assert(!nova_service_manager_complete_stop(&manager, 1U, 11U));
    assert(nova_service_manager_complete_stop(&manager, 1U, 10U));
    assert(nova_service_manager_get(&manager, 1U, &service));
    assert(service.state == NOVA_MANAGED_STOPPED);
    assert(service.task_id == 0U && service.endpoint == 0U);

    /* Failed startup and runtime failure both clear published ownership. */
    assert(nova_service_manager_begin_start(&manager, 1U, 12U));
    assert(nova_service_manager_fail(&manager, 1U, 12U));
    assert(nova_service_manager_get(&manager, 1U, &service));
    assert(service.state == NOVA_MANAGED_FAILED);
    assert(service.task_id == 0U && service.endpoint == 0U);
    assert(nova_service_manager_begin_start(&manager, 1U, 13U));
    assert(nova_service_manager_publish(&manager, 1U, 13U, 21U));
    assert(nova_service_manager_fail(&manager, 1U, 13U));
    assert(!nova_service_manager_get(&manager, 2U, &service));

    puts("User service manager lifecycle tests passed.");
    return 0;
}
