#include "console.h"

#include <stdint.h>

#define PL011_UART0_BASE 0x09000000UL
#define PL011_UARTDR (*(volatile uint32_t *)(PL011_UART0_BASE + 0x00U))
#define PL011_UARTFR (*(volatile uint32_t *)(PL011_UART0_BASE + 0x18U))
#define PL011_UARTFR_TXFF (1U << 5)

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
