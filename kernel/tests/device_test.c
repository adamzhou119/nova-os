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
    struct nova_device_desc no_size = make_device(6U, "arm,bad", 0x1000U, 0U, 37U);
    struct nova_device_desc no_base = make_device(7U, "arm,bad", 0U, 0x1000U, 38U);
    struct nova_device_desc empty_name = make_device(8U, "", 0U, 0U,
                                                      NOVA_DEVICE_NO_IRQ);
    static const char *extra_drivers[] = {
        "test,driver1", "test,driver2", "test,driver3", "test,driver4",
        "test,driver5", "test,driver6", "test,driver7"
    };
    size_t i;

    assert(!nova_device_add(0));
    assert(!nova_device_add(&invalid));
    assert(!nova_device_add(&no_size));
    assert(!nova_device_add(&no_base));
    assert(!nova_device_add(&empty_name));
    assert(nova_device_add(&a));
    assert(!nova_device_add(&a));
    assert(nova_device_add(&overlap));
    assert(nova_device_add(&shared_irq));
    assert(nova_device_add(&b));
    assert(nova_driver_register("arm,pl011", probe, remove_device));
    assert(!nova_driver_register("arm,pl011", probe, remove_device));
    assert(!nova_device_probe(99U));
    for (i = 0; i < sizeof(extra_drivers) / sizeof(extra_drivers[0]); ++i)
        assert(nova_driver_register(extra_drivers[i], probe, remove_device));
    assert(!nova_driver_register("test,driver-overflow", probe, remove_device));

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
    assert(remove_calls == 3);

    /* Fill the bounded device table exactly; the next descriptor is rejected. */
    for (i = 0; i < 12U; ++i) {
        struct nova_device_desc extra = make_device(
            (uint32_t)(10U + i), "arm,unbound", 0U, 0U, NOVA_DEVICE_NO_IRQ);
        assert(nova_device_add(&extra));
    }
    {
        struct nova_device_desc overflow = make_device(
            22U, "arm,unbound", 0U, 0U, NOVA_DEVICE_NO_IRQ);
        assert(!nova_device_add(&overflow));
    }
    puts("Device registry host tests passed.");
    return 0;
}
