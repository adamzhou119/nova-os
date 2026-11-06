#ifndef NOVA_SERVICE_MANAGER_H
#define NOVA_SERVICE_MANAGER_H
#include <stdint.h>
#define NOVA_MANAGER_MAX_SERVICES 8U
enum nova_managed_service_state { NOVA_MANAGED_STOPPED=0, NOVA_MANAGED_STARTING, NOVA_MANAGED_RUNNING, NOVA_MANAGED_STOPPING, NOVA_MANAGED_FAILED };
struct nova_managed_service { uint32_t service_id; uint32_t task_id; uint32_t endpoint; enum nova_managed_service_state state; };
struct nova_service_manager { struct nova_managed_service services[NOVA_MANAGER_MAX_SERVICES]; };
void nova_service_manager_init(struct nova_service_manager *manager);
int nova_service_manager_register(struct nova_service_manager *manager,uint32_t service_id);
int nova_service_manager_begin_start(struct nova_service_manager *manager,uint32_t service_id,uint32_t task_id);
int nova_service_manager_publish(struct nova_service_manager *manager,uint32_t service_id,uint32_t task_id,uint32_t endpoint);
int nova_service_manager_begin_stop(struct nova_service_manager *manager,uint32_t service_id,uint32_t task_id);
int nova_service_manager_complete_stop(struct nova_service_manager *manager,uint32_t service_id,uint32_t task_id);
int nova_service_manager_fail(struct nova_service_manager *manager,uint32_t service_id,uint32_t task_id);
int nova_service_manager_get(const struct nova_service_manager *manager,uint32_t service_id,struct nova_managed_service *service);
#endif
