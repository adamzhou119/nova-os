#include "manager.h"

#include <stddef.h>

static struct nova_managed_service *find_service(
    struct nova_service_manager *manager, uint32_t service_id) {
    size_t i;

    if (manager == 0 || service_id == 0U) return 0;
    for (i = 0U; i < NOVA_MANAGER_MAX_SERVICES; ++i) {
        if (manager->services[i].service_id == service_id)
            return &manager->services[i];
    }
    return 0;
}

void nova_service_manager_init(struct nova_service_manager *manager) {
    size_t i;

    if (manager == 0) return;
    for (i = 0U; i < NOVA_MANAGER_MAX_SERVICES; ++i) {
        manager->services[i].service_id = 0U;
        manager->services[i].task_id = 0U;
        manager->services[i].endpoint = 0U;
        manager->services[i].state = NOVA_MANAGED_STOPPED;
    }
}

int nova_service_manager_register(struct nova_service_manager *manager,
                                  uint32_t service_id) {
    size_t i;

    if (manager == 0 || service_id == 0U ||
        find_service(manager, service_id) != 0) return 0;

    for (i = 0U; i < NOVA_MANAGER_MAX_SERVICES; ++i) {
        struct nova_managed_service *service = &manager->services[i];

        if (service->service_id != 0U) continue;
        service->service_id = service_id;
        service->task_id = 0U;
        service->endpoint = 0U;
        service->state = NOVA_MANAGED_STOPPED;
        return 1;
    }
    return 0;
}

int nova_service_manager_begin_start(struct nova_service_manager *manager,
                                     uint32_t service_id, uint32_t task_id) {
    struct nova_managed_service *service = find_service(manager, service_id);

    if (service == 0 || task_id == 0U ||
        (service->state != NOVA_MANAGED_STOPPED &&
         service->state != NOVA_MANAGED_FAILED)) return 0;

    service->task_id = task_id;
    service->endpoint = 0U;
    service->state = NOVA_MANAGED_STARTING;
    return 1;
}

int nova_service_manager_publish(struct nova_service_manager *manager,
                                 uint32_t service_id, uint32_t task_id,
                                 uint32_t endpoint) {
    struct nova_managed_service *service = find_service(manager, service_id);

    if (service == 0 || task_id == 0U || endpoint == 0U ||
        service->state != NOVA_MANAGED_STARTING ||
        service->task_id != task_id) return 0;

    service->endpoint = endpoint;
    service->state = NOVA_MANAGED_RUNNING;
    return 1;
}

int nova_service_manager_begin_stop(struct nova_service_manager *manager,
                                    uint32_t service_id, uint32_t task_id) {
    struct nova_managed_service *service = find_service(manager, service_id);

    if (service == 0 || task_id == 0U ||
        service->state != NOVA_MANAGED_RUNNING ||
        service->task_id != task_id) return 0;

    service->state = NOVA_MANAGED_STOPPING;
    return 1;
}

int nova_service_manager_complete_stop(struct nova_service_manager *manager,
                                       uint32_t service_id,
                                       uint32_t task_id) {
    struct nova_managed_service *service = find_service(manager, service_id);

    if (service == 0 || task_id == 0U ||
        service->state != NOVA_MANAGED_STOPPING ||
        service->task_id != task_id) return 0;

    service->task_id = 0U;
    service->endpoint = 0U;
    service->state = NOVA_MANAGED_STOPPED;
    return 1;
}

int nova_service_manager_fail(struct nova_service_manager *manager,
                              uint32_t service_id, uint32_t task_id) {
    struct nova_managed_service *service = find_service(manager, service_id);

    if (service == 0 || task_id == 0U || service->task_id != task_id ||
        (service->state != NOVA_MANAGED_STARTING &&
         service->state != NOVA_MANAGED_RUNNING &&
         service->state != NOVA_MANAGED_STOPPING)) return 0;

    service->task_id = 0U;
    service->endpoint = 0U;
    service->state = NOVA_MANAGED_FAILED;
    return 1;
}

int nova_service_manager_get(const struct nova_service_manager *manager,
                             uint32_t service_id,
                             struct nova_managed_service *out) {
    size_t i;

    if (manager == 0 || service_id == 0U || out == 0) return 0;
    for (i = 0U; i < NOVA_MANAGER_MAX_SERVICES; ++i) {
        if (manager->services[i].service_id != service_id) continue;
        *out = manager->services[i];
        return 1;
    }
    return 0;
}
