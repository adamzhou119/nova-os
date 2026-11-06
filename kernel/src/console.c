#include "console.h"
#include "device.h"

#include <stdint.h>

#define PL011_UART0_BASE 0x09000000UL
#define NOVA_PL011_DEVICE_ID 1U

static uintptr_t pl011_base = PL011_UART0_BASE;
#define PL011_UARTDR (*(volatile uint32_t *)(pl011_base + 0x00U))
#define PL011_UARTFR (*(volatile uint32_t *)(pl011_base + 0x18U))
#define PL011_UARTFR_TXFF (1U << 5)


static int pl011_probe(const struct nova_device_desc *device) {
    if (device == 0 || device->mmio_base == 0U || device->mmio_size < 0x20U)
        return 0;
    pl011_base = device->mmio_base;
    return 1;
}

static int pl011_remove(const struct nova_device_desc *device) {
    if (device == 0 || pl011_base != device->mmio_base) return 0;
    pl011_base = PL011_UART0_BASE;
    return 1;
}

int nova_console_init(uintptr_t dtb_address) {
    if (!nova_device_discover_fdt(dtb_address, NOVA_PL011_DEVICE_ID,
                                  "arm,pl011")) {
        nova_console_write("ERROR: PL011 FDT device discovery/IRQ resolution failed.\n");
        return 0;
    }
    nova_console_write("PL011 FDT device descriptor created.\n");
    if (!nova_driver_register("arm,pl011", pl011_probe, pl011_remove)) {
        nova_console_write("ERROR: PL011 driver registration failed.\n");
        return 0;
    }
    nova_console_write("PL011 driver registered.\n");
    if (!nova_device_probe(NOVA_PL011_DEVICE_ID)) {
        nova_console_write("ERROR: PL011 driver Probe failed.\n");
        return 0;
    }
    nova_console_write("PL011 driver Probe succeeded.\n");
    return 1;
}

void nova_console_putc(char value) {
    while (PL011_UARTFR & PL011_UARTFR_TXFF)
        __asm__ volatile("nop");
    PL011_UARTDR = (uint32_t)(unsigned char)value;
}

void nova_console_write(const char *text) {
    if (text == 0) return;
    while (*text != '\0') {
        if (*text == '\n') nova_console_putc('\r');
        nova_console_putc(*text++);
    }
}

void nova_console_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    nova_console_write("0x");
    for (shift = 60; shift >= 0; shift -= 4)
        nova_console_putc(digits[(value >> shift) & 0x0fU]);
    nova_console_putc('\n');
}

void nova_console_write_current_el(uint64_t current_el) {
    nova_console_write("CurrentEL: EL");
    nova_console_putc((char)('0' + (current_el & 0x3U)));
    nova_console_putc('\n');
}
