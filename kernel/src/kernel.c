/* Minimal AArch64 kernel entry with PL011 UART output for QEMU virt. */
#include <stdint.h>

#define UART0_BASE  0x09000000UL
#define UARTDR      (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UARTFR      (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UARTFR_TXFF (1U << 5)

static void uart_putc(char c) {
    while (UARTFR & UARTFR_TXFF) {
        __asm__ volatile("nop");
    }
    UARTDR = (uint32_t)c;
}

static void uart_write(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

void kernel_main(void) {
    uart_write("Welcome to Nova OS\n");
    uart_write("AArch64 kernel prototype is running.\n");
    for (;;) __asm__ volatile("wfe");
}
