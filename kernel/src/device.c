#include "device.h"
#include "fdt.h"

#include <stddef.h>

struct device_slot {
    struct nova_device_info info;
    int used;
};

struct driver_slot {
    char compatible[NOVA_DEVICE_COMPATIBLE_MAX];
    nova_device_probe_fn probe;
    nova_device_remove_fn remove;
    int used;
};

static struct device_slot devices[NOVA_DEVICE_MAX];
static struct driver_slot drivers[NOVA_DRIVER_MAX];

static size_t bounded_length(const char *text, size_t limit) {
    size_t i;
    if (text == 0) return limit;
    for (i = 0; i < limit; ++i)
        if (text[i] == '\0') return i;
    return limit;
}

static int copy_compatible(char destination[NOVA_DEVICE_COMPATIBLE_MAX],
                           const char *source) {
    size_t length = bounded_length(source, NOVA_DEVICE_COMPATIBLE_MAX);
    size_t i;
    if (length == 0U || length >= NOVA_DEVICE_COMPATIBLE_MAX) return 0;
    for (i = 0; i <= length; ++i) destination[i] = source[i];
    return 1;
}

static int strings_equal(const char *left, const char *right) {
    size_t i = 0U;
    if (left == 0 || right == 0) return 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return 0;
        ++i;
    }
    return left[i] == right[i];
}

static struct device_slot *find_device(uint32_t id) {
    size_t i;
    if (id == 0U) return 0;
    for (i = 0; i < NOVA_DEVICE_MAX; ++i)
        if (devices[i].used && devices[i].info.desc.id == id)
            return &devices[i];
    return 0;
}

static struct driver_slot *find_driver(const char *compatible) {
    size_t i;
    for (i = 0; i < NOVA_DRIVER_MAX; ++i)
        if (drivers[i].used &&
            strings_equal(drivers[i].compatible, compatible))
            return &drivers[i];
    return 0;
}

static int resources_conflict(const struct nova_device_desc *a,
                              const struct nova_device_desc *b) {
    uintptr_t a_end, b_end;
    if (a->mmio_size != 0U && b->mmio_size != 0U) {
        a_end = a->mmio_base + a->mmio_size;
        b_end = b->mmio_base + b->mmio_size;
        if (a->mmio_base < b_end && b->mmio_base < a_end) return 1;
    }
    return a->irq != NOVA_DEVICE_NO_IRQ && a->irq == b->irq;
}

static int resources_available(const struct device_slot *candidate) {
    size_t i;
    for (i = 0; i < NOVA_DEVICE_MAX; ++i) {
        if (&devices[i] == candidate || !devices[i].used) continue;
        if (devices[i].info.state == NOVA_DEVICE_PROBING ||
            devices[i].info.state == NOVA_DEVICE_READY) {
            if (resources_conflict(&candidate->info.desc,
                                   &devices[i].info.desc))
                return 0;
        }
    }
    return 1;
}

int nova_device_add(const struct nova_device_desc *device) {
    size_t i;
    struct device_slot *slot = 0;
    char compatible[NOVA_DEVICE_COMPATIBLE_MAX];

    if (device == 0 || device->id == 0U ||
        ((device->mmio_base == 0U) != (device->mmio_size == 0U)) ||
        (device->mmio_size != 0U &&
         device->mmio_base > UINTPTR_MAX - device->mmio_size) ||
        !copy_compatible(compatible, device->compatible) ||
        find_device(device->id) != 0)
        return 0;
    for (i = 0; i < NOVA_DEVICE_MAX; ++i)
        if (!devices[i].used) { slot = &devices[i]; break; }
    if (slot == 0) return 0;

    slot->info.desc = *device;
    for (i = 0; compatible[i] != '\0'; ++i)
        slot->info.desc.compatible[i] = compatible[i];
    slot->info.desc.compatible[i] = '\0';
    slot->info.state = NOVA_DEVICE_DISCOVERED;
    slot->used = 1;
    return 1;
}

int nova_driver_register(const char *compatible,
                         nova_device_probe_fn probe,
                         nova_device_remove_fn remove) {
    size_t i;
    struct driver_slot *slot = 0;
    char name[NOVA_DEVICE_COMPATIBLE_MAX];

    if (probe == 0 || remove == 0 || !copy_compatible(name, compatible) ||
        find_driver(name) != 0)
        return 0;
    for (i = 0; i < NOVA_DRIVER_MAX; ++i)
        if (!drivers[i].used) { slot = &drivers[i]; break; }
    if (slot == 0) return 0;
    for (i = 0; name[i] != '\0'; ++i) slot->compatible[i] = name[i];
    slot->compatible[i] = '\0';
    slot->probe = probe;
    slot->remove = remove;
    slot->used = 1;
    return 1;
}

int nova_device_probe(uint32_t device_id) {
    struct device_slot *device = find_device(device_id);
    struct driver_slot *driver;
    if (device == 0 ||
        (device->info.state != NOVA_DEVICE_DISCOVERED &&
         device->info.state != NOVA_DEVICE_FAILED))
        return 0;
    driver = find_driver(device->info.desc.compatible);
    if (driver == 0 || !resources_available(device)) return 0;

    device->info.state = NOVA_DEVICE_PROBING;
    if (!driver->probe(&device->info.desc)) {
        /* Failed probe owns no resources; a later probe may retry safely. */
        device->info.state = NOVA_DEVICE_FAILED;
        return 0;
    }
    device->info.state = NOVA_DEVICE_READY;
    return 1;
}

int nova_device_remove(uint32_t device_id) {
    struct device_slot *device = find_device(device_id);
    struct driver_slot *driver;
    if (device == 0 || device->info.state != NOVA_DEVICE_READY) return 0;
    driver = find_driver(device->info.desc.compatible);
    if (driver == 0 || !driver->remove(&device->info.desc)) return 0;
    device->info.state = NOVA_DEVICE_DISCOVERED;
    return 1;
}

int nova_device_get(uint32_t device_id, struct nova_device_info *info) {
    struct device_slot *device = find_device(device_id);
    if (device == 0 || info == 0) return 0;
    *info = device->info;
    return 1;
}


int nova_device_discover_fdt_instance(uintptr_t dtb_address, uint32_t device_id,
                                      const char *compatible,
                                      size_t instance_index) {
    struct nova_fdt_device fdt_device;
    struct nova_device_desc desc;
    uint32_t irq = NOVA_DEVICE_NO_IRQ;
    int result;
    size_t i;
    if (device_id == 0U || compatible == NULL || compatible[0] == '\0') return 0;
    result = nova_fdt_find_device_instance(dtb_address, compatible, instance_index,
                                          0U, &fdt_device);
    if (result != 1) return 0;
    result = nova_fdt_find_gicv2_irq_instance(dtb_address, compatible,
                                              instance_index, &irq);
    if (result < 0) return 0;
    for (i = 0U; compatible[i] != '\0' && i < NOVA_DEVICE_COMPATIBLE_MAX; ++i)
        desc.compatible[i] = compatible[i];
    if (i == 0U || i >= NOVA_DEVICE_COMPATIBLE_MAX) return 0;
    desc.compatible[i] = '\0';
    desc.id = device_id;
    desc.mmio_base = fdt_device.mmio_base;
    desc.mmio_size = fdt_device.mmio_size;
    desc.irq = result == 1 ? irq : NOVA_DEVICE_NO_IRQ;
    return nova_device_add(&desc);
}

static void discard_discovered_device(uint32_t device_id) {
    struct device_slot *slot = find_device(device_id);
    if (slot != 0 && slot->info.state == NOVA_DEVICE_DISCOVERED) {
        slot->used = 0;
    }
}

int nova_device_discover_fdt_all(uintptr_t dtb_address,
                                 uint32_t first_device_id,
                                 const char *compatible,
                                 size_t *discovered_count) {
    struct nova_device_desc candidates[NOVA_DEVICE_MAX];
    char compatible_copy[NOVA_DEVICE_COMPATIBLE_MAX];
    size_t count = 0U, free_slots = 0U, i;
    int result;

    if (discovered_count == NULL ||
        !copy_compatible(compatible_copy, compatible)) return 0;
    *discovered_count = 0U;

    /* Discover and validate the complete set before mutating the registry. */
    for (i = 0U; i <= NOVA_DEVICE_MAX; ++i) {
        struct nova_fdt_device fdt_device;
        uint32_t irq = NOVA_DEVICE_NO_IRQ;
        struct nova_device_desc *desc;
        result = nova_fdt_find_device_instance(dtb_address, compatible,
                                                i, 0U, &fdt_device);
        if (result == 0) break;
        if (result < 0 || i == NOVA_DEVICE_MAX ||
            first_device_id == 0U ||
            first_device_id > UINT32_MAX - (uint32_t)i)
            return 0;
        if (find_device(first_device_id + (uint32_t)i) != 0) return 0;

        result = nova_fdt_find_gicv2_irq_instance(dtb_address, compatible,
                                                   i, &irq);
        if (result < 0) return 0;

        desc = &candidates[count];
        desc->id = first_device_id + (uint32_t)i;
        desc->mmio_base = fdt_device.mmio_base;
        desc->mmio_size = fdt_device.mmio_size;
        desc->irq = result == 1 ? irq : NOVA_DEVICE_NO_IRQ;
        for (free_slots = 0U; compatible_copy[free_slots] != '\\0';
             ++free_slots)
            desc->compatible[free_slots] = compatible_copy[free_slots];
        desc->compatible[free_slots] = '\\0';
        ++count;
    }

    if (count == 0U) return 1;
    free_slots = 0U;
    for (i = 0U; i < NOVA_DEVICE_MAX; ++i)
        if (!devices[i].used) ++free_slots;
    if (free_slots < count) return 0;

    for (i = 0U; i < count; ++i) {
        if (!nova_device_add(&candidates[i])) {
            while (i > 0U) {
                --i;
                discard_discovered_device(candidates[i].id);
            }
            return 0;
        }
    }
    *discovered_count = count;
    return 1;
}

int nova_device_discover_fdt(uintptr_t dtb_address, uint32_t device_id,
                             const char *compatible) {
    return nova_device_discover_fdt_instance(dtb_address, device_id,
                                             compatible, 0U);
}
