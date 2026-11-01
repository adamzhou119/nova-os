#ifndef NOVA_DEVICE_H
#define NOVA_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_DEVICE_MAX 16U
#define NOVA_DRIVER_MAX 8U
#define NOVA_DEVICE_COMPATIBLE_MAX 64U
#define NOVA_DEVICE_NO_IRQ UINT32_MAX

enum nova_device_state {
    NOVA_DEVICE_DISCOVERED = 0,
    NOVA_DEVICE_PROBING,
    NOVA_DEVICE_READY,
    NOVA_DEVICE_FAILED
};

struct nova_device_desc {
    uint32_t id;
    char compatible[NOVA_DEVICE_COMPATIBLE_MAX];
    uintptr_t mmio_base;
    size_t mmio_size;
    uint32_t irq;
};

typedef int (*nova_device_probe_fn)(const struct nova_device_desc *device);
typedef int (*nova_device_remove_fn)(const struct nova_device_desc *device);

struct nova_device_info {
    struct nova_device_desc desc;
    enum nova_device_state state;
};

int nova_device_add(const struct nova_device_desc *device);
int nova_device_discover_fdt(uintptr_t dtb_address, uint32_t device_id,
                             const char *compatible);
int nova_driver_register(const char *compatible,
                         nova_device_probe_fn probe,
                         nova_device_remove_fn remove);
int nova_device_probe(uint32_t device_id);
int nova_device_remove(uint32_t device_id);
int nova_device_get(uint32_t device_id, struct nova_device_info *info);

#endif
