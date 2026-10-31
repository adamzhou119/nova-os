/*
 * Nova OS — minimal AArch64 kernel entry.
 * PL011 UART output for the QEMU virt experimental target.
 */
#include <stdint.h>

#define UART0_BASE       0x09000000UL
#define UARTDR           (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UARTFR           (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UARTFR_TXFF      (1U << 5)

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

static void uart_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    uart_write("0x");
    for (shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(value >> shift) & 0x0fU]);
    }
    uart_putc('\n');
}

static void uart_write_current_el(uint64_t current_el) {
    uart_write("CurrentEL: EL");
    uart_putc((char)('0' + (current_el & 0x3U)));
    uart_putc('\n');
}

/* Called from the non-returning assembly exception entry. */
void exception_report(uint64_t esr, uint64_t elr) {
    uart_write("EXCEPTION: EL1 vector captured.\n");
    uart_write("ESR_EL1: ");
    uart_write_hex64(esr);
    uart_write("ELR_EL1: ");
    uart_write_hex64(elr);
    uart_write("System halted for diagnosis.\n");

    for (;;) {
        __asm__ volatile("wfe");
    }
}

void kernel_main(uint64_t current_el) {
    uart_write("Welcome to Nova OS\n");
    uart_write("AArch64 kernel prototype is running.\n");
    uart_write_current_el(current_el);

    if (bss_probe == 0) {
        uart_write("BSS zero initialization verified.\n");
    } else {
        uart_write("ERROR: BSS zero initialization failed.\n");
    }

#ifdef NOVA_TEST_EXCEPTION
    if (current_el == 1) {
        uart_write("Triggering controlled SVC exception test.\n");
        __asm__ volatile("svc #0" ::: "memory");
    }
#endif

    for (;;) {
        __asm__ volatile("wfe");
    }
}
