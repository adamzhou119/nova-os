#include "mmu.h"

#include <stddef.h>

#define NOVA_MMU_TABLE_ENTRIES 512U
#define NOVA_MMU_PAGE_SIZE 4096U
#define NOVA_MMU_BLOCK_2M 0x00200000ULL
#define NOVA_MMU_UART_BASE 0x09000000ULL
#define NOVA_MMU_RAM_BASE 0x40000000ULL
#define NOVA_MMU_RAM_SIZE 0x08000000ULL
#define NOVA_MMU_L3_TABLES (NOVA_MMU_RAM_SIZE / NOVA_MMU_BLOCK_2M)

#define DESC_VALID       (1ULL << 0)
#define DESC_TABLE       (1ULL << 1)
#define DESC_PAGE        (1ULL << 1)
#define DESC_ATTR(n)     ((uint64_t)(n) << 2)
#define DESC_SH_INNER    (3ULL << 8)
#define DESC_SH_OUTER    (2ULL << 8)
#define DESC_AP_RO       (2ULL << 6)
#define DESC_AP_MASK     (3ULL << 6)
#define DESC_AF          (1ULL << 10)
#define DESC_PXN         (1ULL << 53)
#define DESC_UXN         (1ULL << 54)
#define DESC_ADDR_MASK   0x0000FFFFFFFFF000ULL

static uint64_t level1_table[NOVA_MMU_TABLE_ENTRIES]
    __attribute__((aligned(NOVA_MMU_PAGE_SIZE)));
static uint64_t level2_low_table[NOVA_MMU_TABLE_ENTRIES]
    __attribute__((aligned(NOVA_MMU_PAGE_SIZE)));
static uint64_t level2_ram_table[NOVA_MMU_TABLE_ENTRIES]
    __attribute__((aligned(NOVA_MMU_PAGE_SIZE)));
static uint64_t level3_ram_tables[NOVA_MMU_L3_TABLES][NOVA_MMU_TABLE_ENTRIES]
    __attribute__((aligned(NOVA_MMU_PAGE_SIZE)));

extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];

static int set_page_range(uintptr_t start, uintptr_t end,
                          uint64_t set_flags, uint64_t clear_flags) {
    uintptr_t address;
    if (start > end || start < NOVA_MMU_RAM_BASE ||
        end > NOVA_MMU_RAM_BASE + NOVA_MMU_RAM_SIZE ||
        (start & (NOVA_MMU_PAGE_SIZE - 1U)) != 0 ||
        (end & (NOVA_MMU_PAGE_SIZE - 1U)) != 0) return 0;

    for (address = start; address < end; address += NOVA_MMU_PAGE_SIZE) {
        uintptr_t offset = address - NOVA_MMU_RAM_BASE;
        size_t l2_index = (size_t)(offset / NOVA_MMU_BLOCK_2M);
        size_t l3_index = (size_t)((offset % NOVA_MMU_BLOCK_2M) /
                                   NOVA_MMU_PAGE_SIZE);
        uint64_t *descriptor = &level3_ram_tables[l2_index][l3_index];
        if ((*descriptor & DESC_VALID) == 0) return 0;
        *descriptor = (*descriptor & ~clear_flags) | set_flags;
    }
    return 1;
}

static void zero_table(uint64_t *table) {
    size_t i;
    for (i = 0; i < NOVA_MMU_TABLE_ENTRIES; ++i) table[i] = 0;
}

int nova_mmu_enable(void) {
    uint64_t current_el;
    uint64_t mair;
    uint64_t tcr;
    uint64_t sctlr;
    uint64_t device_uart_block;
    uintptr_t l1_address = (uintptr_t)level1_table;
    size_t table_index;
    size_t page_index;

    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (((current_el >> 2) & 0x3U) != 1U) return 0;
    if ((l1_address & (NOVA_MMU_PAGE_SIZE - 1U)) != 0) return 0;

    zero_table(level1_table);
    zero_table(level2_low_table);
    zero_table(level2_ram_table);
    for (table_index = 0; table_index < NOVA_MMU_L3_TABLES; ++table_index)
        zero_table(level3_ram_tables[table_index]);

    /* TTBR0 L1[0] covers low peripherals; L1[1] covers RAM at 0x40000000. */
    level1_table[0] = ((uintptr_t)level2_low_table & DESC_ADDR_MASK) |
                      DESC_VALID | DESC_TABLE;
    level1_table[1] = ((uintptr_t)level2_ram_table & DESC_ADDR_MASK) |
                      DESC_VALID | DESC_TABLE;

    /* QEMU virt PL011: map its 2 MiB region as Device-nGnRnE. */
    device_uart_block = (NOVA_MMU_UART_BASE & ~(NOVA_MMU_BLOCK_2M - 1ULL)) |
                        DESC_VALID | DESC_AF | DESC_SH_OUTER |
                        DESC_PXN | DESC_UXN;
    level2_low_table[(NOVA_MMU_UART_BASE / NOVA_MMU_BLOCK_2M) & 0x1ffU] =
        device_uart_block;

    /*
     * Build 4 KiB leaf mappings for the complete 128 MiB QEMU RAM window.
     * Each L2 entry points to one L3 table containing 512 page descriptors.
     */
    for (table_index = 0; table_index < NOVA_MMU_L3_TABLES; ++table_index) {
        uint64_t table_address = (uintptr_t)level3_ram_tables[table_index];
        level2_ram_table[table_index] = (table_address & DESC_ADDR_MASK) |
                                        DESC_VALID | DESC_TABLE;
        for (page_index = 0; page_index < NOVA_MMU_TABLE_ENTRIES; ++page_index) {
            uint64_t physical = NOVA_MMU_RAM_BASE +
                ((uint64_t)table_index * NOVA_MMU_BLOCK_2M) +
                ((uint64_t)page_index * NOVA_MMU_PAGE_SIZE);
            level3_ram_tables[table_index][page_index] =
                (physical & DESC_ADDR_MASK) | DESC_VALID | DESC_PAGE |
                DESC_ATTR(1) | DESC_SH_INNER | DESC_AF | DESC_PXN | DESC_UXN;
        }
    }

    /*
     * Default RAM pages are EL1 read/write and execute-never.
     * Kernel text becomes EL1 read-only and executable; rodata is read-only
     * and execute-never. Linker boundaries are page aligned.
     */
    if (!set_page_range((uintptr_t)__text_start, (uintptr_t)__text_end,
                        DESC_AP_RO, DESC_AP_MASK | DESC_PXN) ||
        !set_page_range((uintptr_t)__rodata_start, (uintptr_t)__rodata_end,
                        DESC_AP_RO | DESC_PXN, DESC_AP_MASK)) return 0;

    /* AttrIdx 0 = Device-nGnRnE; AttrIdx 1 = Normal memory, WB. */
    mair = 0x000000000000FF00ULL;
    /* 39-bit TTBR0 VA, 4 KiB granule, inner-shareable WB walks. */
    tcr = 25ULL | (1ULL << 8) | (1ULL << 10) | (3ULL << 12) |
          (25ULL << 16) | (1ULL << 23);

    __asm__ volatile(
        "dsb sy\n"
        "msr mair_el1, %0\n"
        "msr tcr_el1, %1\n"
        "msr ttbr0_el1, %2\n"
        "dsb sy\n"
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        :
        : "r"(mair), "r"(tcr), "r"((uint64_t)l1_address)
        : "memory");

    __asm__ volatile(
        "mrs %0, sctlr_el1\n"
        "orr %0, %0, #1\n"
        "orr %0, %0, #4\n"
        "orr %0, %0, #(1 << 12)\n"
        "msr sctlr_el1, %0\n"
        "isb\n"
        : "=&r"(sctlr)
        :
        : "memory");

    return 1;
}

int nova_mmu_verify_identity(uintptr_t address) {
    uint64_t par;
    __asm__ volatile(
        "at s1e1r, %1\n"
        "isb\n"
        "mrs %0, par_el1\n"
        : "=r"(par)
        : "r"((uint64_t)address)
        : "memory");
    if ((par & 1U) != 0) return 0;
    return (par & DESC_ADDR_MASK) ==
           ((uint64_t)address & DESC_ADDR_MASK);
}

int nova_mmu_verify_unmapped(uintptr_t address) {
    uint64_t par;
    __asm__ volatile(
        "at s1e1r, %1\n"
        "isb\n"
        "mrs %0, par_el1\n"
        : "=r"(par)
        : "r"((uint64_t)address)
        : "memory");
    return (par & 1U) != 0;
}

int nova_mmu_verify_writable(uintptr_t address) {
    uint64_t par;
    __asm__ volatile(
        "at s1e1w, %1\n"
        "isb\n"
        "mrs %0, par_el1\n"
        : "=r"(par) : "r"((uint64_t)address) : "memory");
    return (par & 1U) == 0;
}

int nova_mmu_verify_write_protected(uintptr_t address) {
    uint64_t par;
    __asm__ volatile(
        "at s1e1w, %1\n"
        "isb\n"
        "mrs %0, par_el1\n"
        : "=r"(par) : "r"((uint64_t)address) : "memory");
    return (par & 1U) != 0;
}

static uint64_t *lookup_ram_page_descriptor(uintptr_t address) {
    uintptr_t offset;
    size_t l2_index;
    size_t l3_index;
    if (address < NOVA_MMU_RAM_BASE ||
        address >= NOVA_MMU_RAM_BASE + NOVA_MMU_RAM_SIZE) return NULL;
    offset = address - NOVA_MMU_RAM_BASE;
    l2_index = (size_t)(offset / NOVA_MMU_BLOCK_2M);
    l3_index = (size_t)((offset % NOVA_MMU_BLOCK_2M) / NOVA_MMU_PAGE_SIZE);
    return &level3_ram_tables[l2_index][l3_index];
}

int nova_mmu_verify_executable(uintptr_t address) {
    uint64_t *descriptor = lookup_ram_page_descriptor(address);
    return descriptor != NULL && (*descriptor & DESC_VALID) != 0 &&
           (*descriptor & DESC_PXN) == 0;
}

int nova_mmu_verify_execute_never(uintptr_t address) {
    uint64_t *descriptor = lookup_ram_page_descriptor(address);
    return descriptor != NULL && (*descriptor & DESC_VALID) != 0 &&
           (*descriptor & DESC_PXN) != 0;
}
