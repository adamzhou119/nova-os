#include "console.h"
#include "device.h"
#include "fdt.h"

#include <stdint.h>

#define PL011_UART0_BASE NOVA_CONSOLE_UART_PHYS_BASE
#define NOVA_PL011_DEVICE_ID 1U

static uintptr_t pl011_base = PL011_UART0_BASE;
#define PL011_UARTDR (*(volatile uint32_t *)(pl011_base + 0x00U))
#define PL011_UARTFR (*(volatile uint32_t *)(pl011_base + 0x18U))
#define PL011_UARTIBRD (*(volatile uint32_t *)(pl011_base + 0x24U))
#define PL011_UARTFBRD (*(volatile uint32_t *)(pl011_base + 0x28U))
#define PL011_UARTLCR_H (*(volatile uint32_t *)(pl011_base + 0x2CU))
#define PL011_UARTCR (*(volatile uint32_t *)(pl011_base + 0x30U))
#define PL011_UARTFR_BUSY (1U << 3)
#define PL011_TX_WAIT_LIMIT 1000000U
#define PL011_UARTFR_TXFF (1U << 5)
#define PL011_UARTCR_UARTEN (1U << 0)
#define PL011_UARTCR_TXE (1U << 8)
#define PL011_UARTCR_RXE (1U << 9)

static int pl011_configure(void) {
    uint32_t wait;

    /*
     * Do not change divisors or framing while the transmitter is active.
     * A bounded wait prevents a broken clock/device from hanging boot.
     */
    for (wait = 0U; wait < 1000000U; ++wait) {
        if ((PL011_UARTFR & PL011_UARTFR_BUSY) == 0U) break;
        __asm__ volatile("nop");
    }
    if (wait == 1000000U) return 0;

    /* QEMU virt PL011 clock is 24 MHz; divisors select approximately 115200 baud. */
    PL011_UARTCR = 0U;
    __asm__ volatile("dsb sy" ::: "memory");
    PL011_UARTIBRD = 13U;
    PL011_UARTFBRD = 1U;
    __asm__ volatile("dsb sy" ::: "memory");
    PL011_UARTLCR_H = (3U << 5) | (1U << 4); /* 8-bit, FIFO enabled. */
    __asm__ volatile("dsb sy" ::: "memory");
    PL011_UARTCR = PL011_UARTCR_UARTEN | PL011_UARTCR_TXE |
                   PL011_UARTCR_RXE;
    __asm__ volatile("dsb sy" ::: "memory");
    return 1;
}

void nova_console_early_init(void) {
    pl011_base = PL011_UART0_BASE;
    (void)pl011_configure();
}


static int pl011_probe(const struct nova_device_desc *device) {
    uintptr_t previous_base = pl011_base;
    if (device == 0 || device->mmio_base == 0U || device->mmio_size < 0x34U)
        return 0;
    pl011_base = device->mmio_base;
    if (!pl011_configure()) {
        /* Keep the early console mapping if the discovered device is unusable. */
        pl011_base = previous_base;
        return 0;
    }
    return 1;
}

static int pl011_remove(const struct nova_device_desc *device) {
    if (device == 0 || pl011_base != device->mmio_base) return 0;
    pl011_base = PL011_UART0_BASE;
    return 1;
}

int nova_console_init(uintptr_t dtb_address) {
    uint32_t pl011_irq = NOVA_DEVICE_NO_IRQ;
    int irq_result = nova_fdt_find_gicv2_irq(dtb_address, "arm,pl011",
                                             &pl011_irq);
    nova_console_write("FDT PL011 GICv2 IRQ lookup result: ");
    nova_console_write_hex64((uint64_t)(int64_t)irq_result);
    if (irq_result == 1) {
        nova_console_write("FDT PL011 GICv2 IRQ: ");
        nova_console_write_hex64((uint64_t)pl011_irq);
    }
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
    uint32_t wait;
    for (wait = 0U; wait < PL011_TX_WAIT_LIMIT; ++wait) {
        if ((PL011_UARTFR & PL011_UARTFR_TXFF) == 0U) break;
        __asm__ volatile("nop");
    }
    /* A wedged UART must not deadlock the kernel's console path. */
    if (wait == PL011_TX_WAIT_LIMIT) return;
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
