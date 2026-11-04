#ifndef NOVA_SERVICE_H
#define NOVA_SERVICE_H

#include <stdint.h>

#define NOVA_SERVICE_MAX_ENTRIES 16U
#define NOVA_SERVICE_NAME_MAX 31U

/*
 * Kernel-local discovery registry.
 * Lookup reveals an endpoint ID only; it does not grant IPC rights.
 */
int nova_service_register(uint32_t owner, const char *name, uint32_t port);
int nova_service_unregister(uint32_t owner, const char *name);
int64_t nova_service_lookup(const char *name);
void nova_service_close_owner(uint32_t owner);

#endif
