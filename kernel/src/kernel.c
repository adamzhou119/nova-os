/*
 * Nova OS — minimal AArch64 kernel entry.
 * PL011 UART output for the QEMU virt experimental target.
 */
#include <stdint.h>
#include "memory.h"
#include "fdt.h"

#define UART0_BASE       0x09000000UL
#define UARTDR           (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UARTFR           (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UARTFR_TXFF      (1U << 5)

extern char __heap_start[];
extern char __heap_end[];
extern char __page_pool_start[];
extern char __page_pool_end[];
static volatile uint64_t bss_probe;
static struct nova_memory_region detected_memory[NOVA_FDT_MAX_MEMORY_REGIONS];

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

static int test_heap(void) {
    void *small;
    void *page;
    void *too_large;

    if (!nova_heap_init((uintptr_t)__heap_start, (uintptr_t)__heap_end))
        return 0;

    small = nova_alloc(37, 16);
    page = nova_alloc(4096, 4096);
    too_large = nova_alloc(0x100000, 16);

    return small != NULL && page != NULL &&
           (((uintptr_t)small & 15U) == 0) &&
           (((uintptr_t)page & 4095U) == 0) &&
           too_large == NULL && nova_heap_used() <= 0x100000U;
}

static int test_pages(void) {
    void *first;
    void *second;
    void *reused;
    size_t initial;

    if (!nova_page_init((uintptr_t)__page_pool_start,
                        (uintptr_t)__page_pool_end)) {
        uart_write("Page test: initialization failed.\n");
        return 0;
    }

    initial = nova_page_total();
    first = nova_page_alloc();
    second = nova_page_alloc();
    uart_write("Page test first address: ");
    uart_write_hex64((uintptr_t)first);
    uart_write("Page test second address: ");
    uart_write_hex64((uintptr_t)second);

    if (first == NULL || second == NULL) {
        uart_write("Page test: allocation returned NULL.\n");
        return 0;
    }
    if (first == second) {
        uart_write("Page test: duplicate allocation address.\n");
        return 0;
    }
    if ((((uintptr_t)first & (NOVA_PAGE_SIZE - 1U)) != 0) ||
        (((uintptr_t)second & (NOVA_PAGE_SIZE - 1U)) != 0)) {
        uart_write("Page test: returned address is not page aligned.\n");
        return 0;
    }

    if (!nova_page_free(first)) {
        uart_write("Page test: first free failed.\n");
        return 0;
    }
    if (nova_page_free(first)) {
        uart_write("Page test: double free was accepted.\n");
        return 0;
    }
    if (nova_page_free((void *)((uintptr_t)second + 1U))) {
        uart_write("Page test: unaligned free was accepted.\n");
        return 0;
    }

    reused = nova_page_alloc();
    uart_write("Page test reused address: ");
    uart_write_hex64((uintptr_t)reused);
    if (reused != first || nova_page_available() != initial - 2U) {
        uart_write("Page test: freed page was not reused correctly.\n");
        return 0;
    }

    if (!nova_page_free(second) || !nova_page_free(reused)) {
        uart_write("Page test: final free failed.\n");
        return 0;
    }

    if (nova_page_available() != initial) {
        uart_write("Page test: free-page count did not recover.\n");
        return 0;
    }
    uart_write("Page allocator checks passed individually.\n");
    return 1;
}

void kernel_main(uint64_t current_el, uintptr_t dtb_address) {
    int memory_regions;
    uint64_t total_ram = 0;
    size_t region_index;
    uart_write("Welcome to Nova OS\n");
    uart_write("AArch64 kernel prototype is running.\n");
    uart_write_current_el(current_el);

    memory_regions = nova_fdt_memory_regions(dtb_address, detected_memory,
                                             NOVA_FDT_MAX_MEMORY_REGIONS);
    if (memory_regions > 0) {
        for (region_index = 0; region_index < (size_t)memory_regions; ++region_index)
            total_ram += detected_memory[region_index].size;
        uart_write("FDT memory map parsed. Regions: ");
        uart_write_hex64((uint64_t)memory_regions);
        uart_write("FDT total RAM bytes: ");
        uart_write_hex64(total_ram);
    } else {
        uart_write("WARNING: FDT memory map unavailable.\n");
    }

    if (bss_probe == 0) uart_write("BSS zero initialization verified.\n");
    else uart_write("ERROR: BSS zero initialization failed.\n");

    if (test_heap()) uart_write("Heap allocator tests passed.\n");
    else uart_write("ERROR: heap allocator test failed.\n");

    if (test_pages()) uart_write("Physical page allocator tests passed.\n");
    else uart_write("ERROR: physical page allocator test failed.\n");

    uart_write("Page pool total: ");
    uart_write_hex64(nova_page_total());
    uart_write("Page pool available: ");
    uart_write_hex64(nova_page_available());

#ifdef NOVA_TEST_EXCEPTION
    if (current_el == 1) {
        uart_write("Triggering controlled SVC exception test.\n");
        __asm__ volatile("svc #0" ::: "memory");
    }
#endif

    for (;;) __asm__ volatile("wfe");
}
