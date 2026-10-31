/*
 * Nova OS — minimal AArch64 kernel entry.
 * PL011 UART output for the QEMU virt experimental target.
 */
#include <stdint.h>
#include "memory.h"

#define UART0_BASE       0x09000000UL
#define UARTDR           (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UARTFR           (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UARTFR_TXFF      (1U << 5)

extern char __heap_start[];
extern char __heap_end[];
static volatile uint64_t bss_probe;

static void uart_putc(char c) {
    while (UARTFR & UARTFR_TXFF) __asm__ volatile("nop");
    UARTDR = (uint32_t)c;
}

static void uart_write(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    uart_write("0x");
    for (shift = 60; shift >= 0; shift -= 4)
        uart_putc(digits[(value >> shift) & 0x0fU]);
    uart_putc('\n');
}

static void uart_write_current_el(uint64_t current_el) {
    uart_write("CurrentEL: EL");
    uart_putc((char)('0' + (current_el & 0x3U)));
    uart_putc('\n');
}

static const char *exception_vector_name(uint64_t vector) {
    switch (vector) {
    case 0: return "Current EL SP0 synchronous";
    case 1: return "Current EL SP0 IRQ";
    case 2: return "Current EL SP0 FIQ";
    case 3: return "Current EL SP0 SError";
    case 4: return "Current EL SPx synchronous";
    case 5: return "Current EL SPx IRQ";
    case 6: return "Current EL SPx FIQ";
    case 7: return "Current EL SPx SError";
    case 8: return "Lower EL AArch64 synchronous";
    case 9: return "Lower EL AArch64 IRQ";
    case 10: return "Lower EL AArch64 FIQ";
    case 11: return "Lower EL AArch64 SError";
    case 12: return "Lower EL AArch32 synchronous";
    case 13: return "Lower EL AArch32 IRQ";
    case 14: return "Lower EL AArch32 FIQ";
    case 15: return "Lower EL AArch32 SError";
    default: return "Unknown vector";
    }
}

void exception_report(uint64_t esr, uint64_t elr, uint64_t vector,
                      uint64_t far, uint64_t spsr) {
    uart_write("EXCEPTION: EL1 vector captured.\n");
    uart_write("Vector: ");
    uart_write(exception_vector_name(vector));
    uart_putc('\n');
    uart_write("Vector slot: ");
    uart_write_hex64(vector);
    uart_write("ESR_EL1: ");
    uart_write_hex64(esr);
    uart_write("ESR exception class: ");
    uart_write_hex64((esr >> 26) & 0x3fU);
    uart_write("ESR instruction-specific syndrome: ");
    uart_write_hex64(esr & 0x01ffffffU);
    uart_write("ELR_EL1: ");
    uart_write_hex64(elr);
    uart_write("FAR_EL1: ");
    uart_write_hex64(far);
    uart_write("SPSR_EL1: ");
    uart_write_hex64(spsr);
    uart_write("System halted for diagnosis.\n");
    for (;;) __asm__ volatile("wfe");
}

static void test_heap(void) {
    void *small;
    void *page;
    void *too_large;

    if (!nova_heap_init((uintptr_t)__heap_start, (uintptr_t)__heap_end)) {
        uart_write("ERROR: heap initialization failed.\n");
        return;
    }

    small = nova_alloc(37, 16);
    page = nova_alloc(4096, 4096);
    too_large = nova_alloc(0x100000, 16);

    if (small != NULL && page != NULL &&
        (((uintptr_t)small & 15U) == 0) &&
        (((uintptr_t)page & 4095U) == 0) &&
        too_large == NULL &&
        nova_heap_used() <= 0x100000U) {
        uart_write("Heap allocator tests passed.\n");
    } else {
        uart_write("ERROR: heap allocator test failed.\n");
    }

    uart_write("Heap bytes used: ");
    uart_write_hex64(nova_heap_used());
    uart_write("Heap bytes available: ");
    uart_write_hex64(nova_heap_available());
}

void kernel_main(uint64_t current_el) {
    uart_write("Welcome to Nova OS\n");
    uart_write("AArch64 kernel prototype is running.\n");
    uart_write_current_el(current_el);

    if (bss_probe == 0) uart_write("BSS zero initialization verified.\n");
    else uart_write("ERROR: BSS zero initialization failed.\n");

    test_heap();

#ifdef NOVA_TEST_EXCEPTION
    if (current_el == 1) {
        uart_write("Triggering controlled SVC exception test.\n");
        __asm__ volatile("svc #0" ::: "memory");
    }
#endif

    for (;;) __asm__ volatile("wfe");
}
