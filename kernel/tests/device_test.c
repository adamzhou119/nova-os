#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "device.h"

static int probe_calls;
static int remove_calls;
static int fail_probe;
static int fail_remove;

static int probe(const struct nova_device_desc *device) {
    ++probe_calls;
    assert(device != 0 && device->id != 0U);
    return !fail_probe;
}
static int remove_device(const struct nova_device_desc *device) {
    ++remove_calls;
    assert(device != 0 && device->id != 0U);
    return !fail_remove;
}
static struct nova_device_desc make_device(uint32_t id, const char *compatible,
                                            uintptr_t base, size_t size,
                                            uint32_t irq) {
    struct nova_device_desc d = {0};
    size_t i;
    d.id = id;
    for (i = 0; compatible[i] != '\0' && i + 1U < NOVA_DEVICE_COMPATIBLE_MAX; ++i)
        d.compatible[i] = compatible[i];
    d.mmio_base = base;
    d.mmio_size = size;
    d.irq = irq;
    return d;
}

int main(void) {
    struct nova_device_info info;
    struct nova_device_desc a = make_device(1U, "arm,pl011", 0x09000000U, 0x1000U, 33U);
    struct nova_device_desc overlap = make_device(2U, "arm,pl011", 0x09000800U, 0x1000U, 34U);
    struct nova_device_desc shared_irq = make_device(3U, "arm,pl011", 0x09002000U, 0x1000U, 33U);
    struct nova_device_desc b = make_device(4U, "arm,pl011", 0x09002000U, 0x1000U, 36U);
    struct nova_device_desc invalid = make_device(5U, "arm,bad", UINTPTR_MAX - 3U, 8U, 35U);

    assert(!nova_device_add(0));
    assert(!nova_device_add(&invalid));
    assert(nova_device_add(&a));
    assert(!nova_device_add(&a));
    assert(nova_device_add(&overlap));
    assert(nova_device_add(&shared_irq));
    assert(nova_device_add(&b));
    assert(nova_driver_register("arm,pl011", probe, remove_device));
    assert(!nova_driver_register("arm,pl011", probe, remove_device));
    assert(!nova_device_probe(99U));

    fail_probe = 1;
    assert(!nova_device_probe(1U));
    assert(nova_device_get(1U, &info) && info.state == NOVA_DEVICE_FAILED);
    fail_probe = 0;
    /* A failed probe releases its MMIO/IRQ claims for another device. */
    assert(nova_device_probe(2U));
    assert(nova_device_remove(2U));
    assert(nova_device_probe(1U));
    assert(probe_calls == 3);
    assert(nova_device_get(1U, &info) && info.state == NOVA_DEVICE_READY);

    /* Probe failure releases both MMIO and IRQ reservations. */
    assert(!nova_device_probe(2U));
    assert(!nova_device_probe(3U));
    /* Disjoint resources are available while device 1 is ready. */
    assert(nova_device_probe(4U));
    fail_remove = 1;
    assert(!nova_device_remove(1U));
    assert(nova_device_get(1U, &info) && info.state == NOVA_DEVICE_READY);
    fail_remove = 0;
    assert(nova_device_remove(1U));
    assert(nova_device_get(1U, &info) && info.state == NOVA_DEVICE_DISCOVERED);
    assert(nova_device_probe(2U));
    assert(remove_calls == 2);
    puts("Device registry host tests passed.");
    return 0;
}
