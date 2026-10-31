/*
 * Nova OS — minimal AArch64 kernel entry.
 * PL011 UART output for the QEMU virt experimental target.
 */
#include <stdint.h>

#define UART0_BASE       0x09000000UL
#define UARTDR           (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UARTFR           (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UARTFR_TXFF      (1U << 5)

/* This zero-initialized object is a runtime probe for early .bss clearing. */
static volatile uint64_t bss_probe;

static void uart_putc(char c) {
    while (UARTFR & UARTFR_TXFF) {
        __asm__ volatile("nop");
    }
    UARTDR = (uint32_t)c;
}

static void uart_write(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

void kernel_main(void) {
    uart_write("Welcome to Nova OS\n");
    uart_write("AArch64 kernel prototype is running.\n");

    if (bss_probe == 0) {
        uart_write("BSS zero initialization verified.\n");
    } else {
        uart_write("ERROR: BSS zero initialization failed.\n");
    }

    for (;;) {
        __asm__ volatile("wfe");
    }
}
