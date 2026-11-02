#include "mmu.h"

#include <stddef.h>
#include "memory.h"

#define NOVA_MMU_TABLE_ENTRIES 512U
#define NOVA_MMU_PAGE_SIZE 4096U
#define NOVA_MMU_BLOCK_2M 0x00200000ULL
#define NOVA_MMU_UART_BASE 0x09000000ULL
#define NOVA_MMU_GIC_BASE  0x08000000ULL
#define NOVA_MMU_RAM_BASE 0x40000000ULL
#define NOVA_MMU_RAM_SIZE 0x08000000ULL
#define NOVA_MMU_L2_ENTRIES NOVA_MMU_TABLE_ENTRIES
#define NOVA_MMU_L3_TABLES NOVA_MMU_L2_ENTRIES
#define NOVA_MMU_DYNAMIC_VA_BASE 0x48000000ULL
#define NOVA_MMU_DYNAMIC_VA_END  0x80000000ULL
#define NOVA_MMU_MAX_PHYSICAL_PAGES (NOVA_MMU_RAM_SIZE / NOVA_MMU_PAGE_SIZE)

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
/* Track writable/executable aliases per physical frame to enforce global W^X. */
static uint32_t mapped_aliases[NOVA_MMU_MAX_PHYSICAL_PAGES];
static uint32_t writable_aliases[NOVA_MMU_MAX_PHYSICAL_PAGES];
static uint32_t executable_aliases[NOVA_MMU_MAX_PHYSICAL_PAGES];

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
    uint64_t device_gic_block;
    uintptr_t l1_address = (uintptr_t)level1_table;
    size_t table_index;
    size_t page_index;

    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (((current_el >> 2) & 0x3U) != 1U) return 0;
    if ((l1_address & (NOVA_MMU_PAGE_SIZE - 1U)) != 0) return 0;

    for (table_index = 0; table_index < NOVA_MMU_MAX_PHYSICAL_PAGES; ++table_index) {
        mapped_aliases[table_index] = 0;
        writable_aliases[table_index] = 0;
        executable_aliases[table_index] = 0;
    }
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

    /* QEMU virt GICv2 distributor and CPU interface share this device block. */
    device_gic_block = (NOVA_MMU_GIC_BASE & ~(NOVA_MMU_BLOCK_2M - 1ULL)) |
                       DESC_VALID | DESC_AF | DESC_SH_OUTER |
                       DESC_PXN | DESC_UXN;
    level2_low_table[(NOVA_MMU_GIC_BASE / NOVA_MMU_BLOCK_2M) & 0x1ffU] =
        device_gic_block;

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
    for (table_index = 0; table_index < NOVA_MMU_L2_ENTRIES; ++table_index) {
        uint64_t table_address = (uintptr_t)level3_ram_tables[table_index];
        level2_ram_table[table_index] = (table_address & DESC_ADDR_MASK) |
                                        DESC_VALID | DESC_TABLE;
        if (table_index < (NOVA_MMU_RAM_SIZE / NOVA_MMU_BLOCK_2M)) {
            for (page_index = 0; page_index < NOVA_MMU_TABLE_ENTRIES; ++page_index) {
                uint64_t physical = NOVA_MMU_RAM_BASE +
                    ((uint64_t)table_index * NOVA_MMU_BLOCK_2M) +
                    ((uint64_t)page_index * NOVA_MMU_PAGE_SIZE);
                level3_ram_tables[table_index][page_index] =
                    (physical & DESC_ADDR_MASK) | DESC_VALID | DESC_PAGE |
                    DESC_ATTR(1) | DESC_SH_INNER | DESC_AF | DESC_PXN | DESC_UXN;
            }
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
        address >= NOVA_MMU_DYNAMIC_VA_END) return NULL;
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

static int dynamic_va_indices(uintptr_t address, size_t *l2_index,
                              size_t *l3_index) {
    uintptr_t offset;
    if ((address & (NOVA_MMU_PAGE_SIZE - 1U)) != 0 ||
        address < NOVA_MMU_DYNAMIC_VA_BASE ||
        address >= NOVA_MMU_DYNAMIC_VA_END) return 0;
    offset = address - NOVA_MMU_RAM_BASE;
    *l2_index = (size_t)(offset / NOVA_MMU_BLOCK_2M);
    *l3_index = (size_t)((offset % NOVA_MMU_BLOCK_2M) /
                         NOVA_MMU_PAGE_SIZE);
    return *l2_index < NOVA_MMU_L2_ENTRIES;
}

static void invalidate_stage1_page(uintptr_t address) {
    uint64_t operand = (uint64_t)(address >> 12);
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi vae1is, %0\n"
        "dsb ish\n"
        "isb\n"
        : : "r"(operand) : "memory");
}

static size_t physical_frame_index(uintptr_t physical_address) {
    return (size_t)((physical_address - NOVA_MMU_RAM_BASE) /
                    NOVA_MMU_PAGE_SIZE);
}

static int descriptor_is_writable(uint64_t descriptor) {
    return (descriptor & (1ULL << 7)) == 0;
}

static int descriptor_is_executable(uint64_t descriptor) {
    return (descriptor & (DESC_PXN | DESC_UXN)) != (DESC_PXN | DESC_UXN);
}

int nova_mmu_map_page(uintptr_t virtual_address, uintptr_t physical_address,
                      unsigned int flags) {
    size_t l2_index, l3_index, frame_index;
    uint64_t descriptor;
    uint64_t *slot;
    int writable = (flags & NOVA_MMU_MAP_WRITABLE) != 0;
    int executable = (flags & NOVA_MMU_MAP_EXECUTABLE) != 0;
    if ((physical_address & (NOVA_MMU_PAGE_SIZE - 1U)) != 0 ||
        physical_address < NOVA_MMU_RAM_BASE ||
        physical_address >= NOVA_MMU_RAM_BASE + NOVA_MMU_RAM_SIZE ||
        (flags & ~(NOVA_MMU_MAP_WRITABLE | NOVA_MMU_MAP_EXECUTABLE)) != 0 ||
        (writable && executable) ||
        !nova_page_is_allocated((const void *)physical_address) ||
        !dynamic_va_indices(virtual_address, &l2_index, &l3_index)) return 0;

    slot = &level3_ram_tables[l2_index][l3_index];
    if ((*slot & DESC_VALID) != 0) return 0;
    frame_index = physical_frame_index(physical_address);
    if ((writable && executable_aliases[frame_index] != 0) ||
        (executable && writable_aliases[frame_index] != 0)) return 0;
    /* Pin before publishing the PTE: free() must reject mapped frames. */
    if (!nova_page_pin((const void *)physical_address)) return 0;

    descriptor = ((uint64_t)physical_address & DESC_ADDR_MASK) |
                 DESC_VALID | DESC_PAGE | DESC_ATTR(1) |
                 DESC_SH_INNER | DESC_AF | DESC_UXN;
    if (!writable) descriptor |= DESC_AP_RO;
    if (!executable) descriptor |= DESC_PXN;
    *slot = descriptor;
    if (writable) ++writable_aliases[frame_index];
    if (executable) ++executable_aliases[frame_index];
    invalidate_stage1_page(virtual_address);
    return 1;
}

int nova_mmu_unmap_page(uintptr_t virtual_address) {
    size_t l2_index, l3_index, frame_index;
    uint64_t *slot;
    uintptr_t physical_address;
    if (!dynamic_va_indices(virtual_address, &l2_index, &l3_index)) return 0;
    slot = &level3_ram_tables[l2_index][l3_index];
    if ((*slot & DESC_VALID) == 0) return 0;
    physical_address = (uintptr_t)(*slot & DESC_ADDR_MASK);
    frame_index = physical_frame_index(physical_address);
    if (descriptor_is_writable(*slot) && writable_aliases[frame_index] != 0)
        --writable_aliases[frame_index];
    if (descriptor_is_executable(*slot) && executable_aliases[frame_index] != 0)
        --executable_aliases[frame_index];
    *slot = 0;
    invalidate_stage1_page(virtual_address);
    return nova_page_unpin((const void *)physical_address);
}

int nova_mmu_protect_page(uintptr_t virtual_address, unsigned int flags) {
    size_t l2_index, l3_index, frame_index;
    uint64_t *slot;
    uint64_t descriptor;
    uintptr_t physical_address;
    int old_writable, old_executable;
    int new_writable = (flags & NOVA_MMU_MAP_WRITABLE) != 0;
    int new_executable = (flags & NOVA_MMU_MAP_EXECUTABLE) != 0;
    if ((flags & ~(NOVA_MMU_MAP_WRITABLE | NOVA_MMU_MAP_EXECUTABLE)) != 0 ||
        (new_writable && new_executable) ||
        !dynamic_va_indices(virtual_address, &l2_index, &l3_index)) return 0;
    slot = &level3_ram_tables[l2_index][l3_index];
    if ((*slot & DESC_VALID) == 0) return 0;
    physical_address = (uintptr_t)(*slot & DESC_ADDR_MASK);
    frame_index = physical_frame_index(physical_address);
    old_writable = descriptor_is_writable(*slot);
    old_executable = descriptor_is_executable(*slot);
    if ((new_writable && executable_aliases[frame_index] > (size_t)old_executable) ||
        (new_executable && writable_aliases[frame_index] > (size_t)old_writable))
        return 0;

    descriptor = *slot & ~(DESC_AP_MASK | DESC_PXN | DESC_UXN);
    if (!new_writable) descriptor |= DESC_AP_RO;
    if (!new_executable) descriptor |= DESC_PXN;
    descriptor |= DESC_UXN;
    *slot = descriptor;
    if (old_writable && !new_writable) --writable_aliases[frame_index];
    if (!old_writable && new_writable) ++writable_aliases[frame_index];
    if (old_executable && !new_executable) --executable_aliases[frame_index];
    if (!old_executable && new_executable) ++executable_aliases[frame_index];
    invalidate_stage1_page(virtual_address);
    return 1;
}

int nova_mmu_translate(uintptr_t virtual_address,
                       uintptr_t *physical_address) {
    uint64_t par;
    if (physical_address == NULL) return 0;
    __asm__ volatile(
        "at s1e1r, %1\n"
        "isb\n"
        "mrs %0, par_el1\n"
        : "=r"(par) : "r"((uint64_t)virtual_address) : "memory");
    if ((par & 1U) != 0) return 0;
    *physical_address = (uintptr_t)((par & DESC_ADDR_MASK) |
                                    ((uint64_t)virtual_address &
                                     (NOVA_MMU_PAGE_SIZE - 1U)));
    return 1;
}

int nova_mmu_verify_translation(uintptr_t virtual_address,
                                uintptr_t physical_address) {
    uint64_t par;
    __asm__ volatile(
        "at s1e1r, %1\n"
        "isb\n"
        "mrs %0, par_el1\n"
        : "=r"(par) : "r"((uint64_t)virtual_address) : "memory");
    if ((par & 1U) != 0) return 0;
    return (par & DESC_ADDR_MASK) ==
           ((uint64_t)physical_address & DESC_ADDR_MASK);
}


/*
 * Address-space roots share the privileged platform/RAM subtrees, but own
 * L1[2] (VA 0x80000000..0xbfffffff) for future EL0 mappings. The private
 * L2 starts empty, so no user mapping can accidentally inherit kernel access.
 */
uintptr_t nova_mmu_kernel_root(void) {
    return (uintptr_t)level1_table;
}

int nova_mmu_address_space_create(uintptr_t *root_physical) {
    uint64_t *root;
    uint64_t *user_l2;
    size_t i;
    if (root_physical == NULL) return 0;
    root = (uint64_t *)nova_page_alloc();
    if (root == NULL) return 0;
    user_l2 = (uint64_t *)nova_page_alloc();
    if (user_l2 == NULL) {
        (void)nova_page_free(root);
        return 0;
    }
    for (i = 0; i < NOVA_MMU_TABLE_ENTRIES; ++i) {
        root[i] = level1_table[i];
        user_l2[i] = 0U;
    }
    root[2] = ((uintptr_t)user_l2 & DESC_ADDR_MASK) |
              DESC_VALID | DESC_TABLE;
    if (!nova_page_pin(root) || !nova_page_pin(user_l2)) {
        if (nova_page_pin_count(root) != 0U) (void)nova_page_unpin(root);
        (void)nova_page_free(user_l2);
        (void)nova_page_free(root);
        return 0;
    }
    *root_physical = (uintptr_t)root;
    return 1;
}

/*
 * Validate the complete ownership shape before dereferencing a private table.
 * A live address space owns exactly one pin on its root, L2, and each L3.
 */
static uint64_t *validated_user_l2(uintptr_t root_physical) {
    uint64_t *root;
    uint64_t *user_l2;
    size_t i;

    if (root_physical == 0U ||
        root_physical == (uintptr_t)level1_table ||
        (root_physical & (NOVA_MMU_PAGE_SIZE - 1U)) != 0U ||
        !nova_page_is_allocated((const void *)root_physical) ||
        nova_page_pin_count((const void *)root_physical) != 1U)
        return NULL;

    root = (uint64_t *)root_physical;
    for (i = 0; i < NOVA_MMU_TABLE_ENTRIES; ++i)
        if (i != 2U && root[i] != level1_table[i]) return NULL;
    if ((root[2] & (DESC_VALID | DESC_TABLE)) !=
        (DESC_VALID | DESC_TABLE)) return NULL;

    user_l2 = (uint64_t *)(uintptr_t)(root[2] & DESC_ADDR_MASK);
    if (!nova_page_is_allocated(user_l2) ||
        nova_page_pin_count(user_l2) != 1U)
        return NULL;
    return user_l2;
}

int nova_mmu_address_space_destroy(uintptr_t root_physical, uint16_t asid) {
    uint64_t *root = (uint64_t *)root_physical;
    uint64_t *user_l2;
    uint64_t current_ttbr0;
    size_t i, j;

    /*
     * The address-space registry tracks software users, but this low-level
     * entry is also callable directly. Never reclaim the root currently
     * installed in TTBR0_EL1, even if a caller bypasses the registry.
     */
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(current_ttbr0));
    if ((current_ttbr0 & DESC_ADDR_MASK) == (uint64_t)root_physical)
        return 0;

    if (asid == 0U || asid > 255U) return 0;
    user_l2 = validated_user_l2(root_physical);
    if (user_l2 == NULL) return 0;

    /*
     * Preflight the complete private user tree before changing a descriptor.
     * An inactive address space owns one pin on each page-table page and one
     * pin on every mapped frame; mapped data frames themselves are not freed.
     */
    for (i = 0; i < NOVA_MMU_TABLE_ENTRIES; ++i) {
        uint64_t *l3;
        if (user_l2[i] == 0U) continue;
        if ((user_l2[i] & (DESC_VALID | DESC_TABLE)) !=
            (DESC_VALID | DESC_TABLE)) return 0;
        l3 = (uint64_t *)(uintptr_t)(user_l2[i] & DESC_ADDR_MASK);
        if (!nova_page_is_allocated(l3) || nova_page_pin_count(l3) != 1U)
            return 0;
        for (j = 0; j < NOVA_MMU_TABLE_ENTRIES; ++j) {
            uint64_t desc = l3[j];
            uintptr_t pa;
            size_t frame;
            if (desc == 0U) continue;
            if ((desc & (DESC_VALID | DESC_PAGE)) !=
                (DESC_VALID | DESC_PAGE)) return 0;
            pa = (uintptr_t)(desc & DESC_ADDR_MASK);
            if (pa < NOVA_MMU_RAM_BASE ||
                pa >= NOVA_MMU_RAM_BASE + NOVA_MMU_RAM_SIZE ||
                !nova_page_is_allocated((const void *)pa)) return 0;
            frame = physical_frame_index(pa);
            if (mapped_aliases[frame] == 0U ||
                nova_page_pin_count((const void *)pa) < mapped_aliases[frame])
                return 0;
            if ((descriptor_is_writable(desc) && writable_aliases[frame] == 0U) ||
                (descriptor_is_executable(desc) && executable_aliases[frame] == 0U))
                return 0;
        }
    }

    /*
     * The registry guarantees no CPU is actively using this root. Flush the
     * complete ASID before releasing any leaf frame or page-table page so a
     * later ASID reuse cannot observe stale translations.
     */
    {
        uint64_t operand = (uint64_t)asid << 48;
        __asm__ volatile(
            "dsb ishst\n"
            "tlbi aside1is, %0\n"
            "dsb ish\n"
            "isb\n"
            : : "r"(operand) : "memory");
    }
    for (i = 0; i < NOVA_MMU_TABLE_ENTRIES; ++i) {
        uint64_t *l3;
        if (user_l2[i] == 0U) continue;
        l3 = (uint64_t *)(uintptr_t)(user_l2[i] & DESC_ADDR_MASK);
        for (j = 0; j < NOVA_MMU_TABLE_ENTRIES; ++j) {
            uint64_t desc = l3[j];
            uintptr_t pa;
            size_t frame;
            if (desc == 0U) continue;
            pa = (uintptr_t)(desc & DESC_ADDR_MASK);
            frame = physical_frame_index(pa);
            if (descriptor_is_writable(desc)) --writable_aliases[frame];
            if (descriptor_is_executable(desc)) --executable_aliases[frame];
            l3[j] = 0U;
            if (!nova_page_unpin((const void *)pa)) return 0;
            --mapped_aliases[frame];
        }
        user_l2[i] = 0U;
        if (!nova_page_unpin(l3) || !nova_page_free(l3)) return 0;
    }

    if (!nova_page_unpin(user_l2) || !nova_page_unpin(root)) return 0;
    if (!nova_page_free(user_l2)) return 0;
    return nova_page_free(root);
}

int nova_mmu_switch_address_space(uintptr_t root_physical, uint16_t asid) {
    uint64_t ttbr;
    uint64_t operand;
    if (asid > 255U ||
        (root_physical == (uintptr_t)level1_table && asid != 0U) ||
        (root_physical != (uintptr_t)level1_table &&
         (asid == 0U || validated_user_l2(root_physical) == NULL)))
        return 0;
    ttbr = ((uint64_t)asid << 48) |
           ((uint64_t)root_physical & DESC_ADDR_MASK);
    operand = (uint64_t)asid << 48;
    __asm__ volatile(
        "dsb ishst\n"
        "tlbi aside1is, %0\n"
        "dsb ish\n"
        "msr ttbr0_el1, %1\n"
        "isb\n"
        : : "r"(operand), "r"(ttbr) : "memory");
    return 1;
}

#define DESC_AP_USER_RW  (1ULL << 6)
#define DESC_AP_USER_RO  (3ULL << 6)
static uint64_t *user_l2_for_root(uintptr_t root_physical) {
    return validated_user_l2(root_physical);
}
static int user_va_indices(uintptr_t va,size_t *l2,size_t *l3) {
    uintptr_t off;
    if ((va&(NOVA_MMU_PAGE_SIZE-1U))!=0U || va<NOVA_USER_VA_BASE || va>=NOVA_USER_VA_END) return 0;
    off=va-NOVA_USER_VA_BASE; *l2=(size_t)(off/NOVA_MMU_BLOCK_2M);
    *l3=(size_t)((off%NOVA_MMU_BLOCK_2M)/NOVA_MMU_PAGE_SIZE);
    return *l2<NOVA_MMU_TABLE_ENTRIES;
}
static void invalidate_user_page(uintptr_t va,uint16_t asid) {
    uint64_t op=((uint64_t)asid<<48)|((uint64_t)va>>12);
    __asm__ volatile("dsb ishst\ntlbi vae1is, %0\ndsb ish\nisb"::"r"(op):"memory");
}
int nova_mmu_user_map_page(uintptr_t root_physical,uint16_t asid,uintptr_t va,
                           uintptr_t pa,unsigned int flags) {
    uint64_t *l2,*l3,*slot,desc; size_t i2,i3,fi;
    int created_l3=0;
    int wr=(flags&NOVA_MMU_MAP_WRITABLE)!=0U, ex=(flags&NOVA_MMU_MAP_EXECUTABLE)!=0U;
    if(asid==0U||asid>255U||(flags&~(NOVA_MMU_MAP_WRITABLE|NOVA_MMU_MAP_EXECUTABLE))!=0U||
       (wr&&ex)||(pa&(NOVA_MMU_PAGE_SIZE-1U))!=0U||pa<NOVA_MMU_RAM_BASE||
       pa>=NOVA_MMU_RAM_BASE+NOVA_MMU_RAM_SIZE||!nova_page_is_allocated((const void*)pa)||
       !user_va_indices(va,&i2,&i3)) return 0;
    l2=user_l2_for_root(root_physical); if(l2==NULL)return 0;
    if((l2[i2]&DESC_VALID)==0U){
        l3=(uint64_t*)nova_page_alloc(); if(l3==NULL)return 0; zero_table(l3);
        if(!nova_page_pin(l3)){(void)nova_page_free(l3);return 0;}
        l2[i2]=((uintptr_t)l3&DESC_ADDR_MASK)|DESC_VALID|DESC_TABLE;
        created_l3=1;
    } else {
        if((l2[i2]&DESC_TABLE)==0U)return 0;
        l3=(uint64_t*)(uintptr_t)(l2[i2]&DESC_ADDR_MASK);
        if(!nova_page_is_allocated(l3))return 0;
    }
    slot=&l3[i3]; fi=physical_frame_index(pa);
    if((*slot&DESC_VALID)!=0U ||
       (wr&&executable_aliases[fi]!=0U) || (ex&&writable_aliases[fi]!=0U) ||
       mapped_aliases[fi]==UINT32_MAX ||
       (wr&&writable_aliases[fi]==UINT32_MAX) ||
       (ex&&executable_aliases[fi]==UINT32_MAX) ||
       !nova_page_pin((const void*)pa)) {
        if(created_l3) {
            l2[i2]=0U;
            (void)nova_page_unpin(l3);
            (void)nova_page_free(l3);
        }
        return 0;
    }
    desc=((uint64_t)pa&DESC_ADDR_MASK)|DESC_VALID|DESC_PAGE|DESC_ATTR(1)|
         DESC_SH_INNER|DESC_AF|DESC_PXN|(wr?DESC_AP_USER_RW:DESC_AP_USER_RO);
    if(!ex)desc|=DESC_UXN;
    *slot=desc; ++mapped_aliases[fi];
    if(wr)++writable_aliases[fi];
    if(ex)++executable_aliases[fi];
    invalidate_user_page(va,asid); return 1;
}
int nova_mmu_user_unmap_page(uintptr_t root_physical,uint16_t asid,uintptr_t va) {
    uint64_t *l2,*l3,*slot; uintptr_t pa; size_t i2,i3,i,fi; int empty=1;
    if(asid==0U||asid>255U||!user_va_indices(va,&i2,&i3))return 0;
    l2=user_l2_for_root(root_physical);
    if(l2==NULL||(l2[i2]&(DESC_VALID|DESC_TABLE))!=(DESC_VALID|DESC_TABLE))return 0;
    l3=(uint64_t*)(uintptr_t)(l2[i2]&DESC_ADDR_MASK); if(!nova_page_is_allocated(l3))return 0;
    slot=&l3[i3]; if((*slot&DESC_VALID)==0U)return 0;
    pa=(uintptr_t)(*slot&DESC_ADDR_MASK);
    if(pa<NOVA_MMU_RAM_BASE || pa>=NOVA_MMU_RAM_BASE+NOVA_MMU_RAM_SIZE)
        return 0;
    fi=physical_frame_index(pa);
    if(!nova_page_is_allocated((const void*)pa) ||
       nova_page_pin_count((const void*)pa)==0U || mapped_aliases[fi]==0U ||
       (descriptor_is_writable(*slot)&&writable_aliases[fi]==0U) ||
       (descriptor_is_executable(*slot)&&executable_aliases[fi]==0U))
        return 0;
    if(descriptor_is_writable(*slot))--writable_aliases[fi];
    if(descriptor_is_executable(*slot))--executable_aliases[fi];
    *slot=0U; invalidate_user_page(va,asid);
    if(!nova_page_unpin((const void*)pa))return 0;
    --mapped_aliases[fi];
    for(i=0;i<NOVA_MMU_TABLE_ENTRIES;++i)if(l3[i]!=0U){empty=0;break;}
    if(empty){l2[i2]=0U;if(!nova_page_unpin(l3))return 0;return nova_page_free(l3);}
    return 1;
}
int nova_mmu_verify_user_access(uintptr_t va,int write) {
    uint64_t par; if(va<NOVA_USER_VA_BASE||va>=NOVA_USER_VA_END)return 0;
    if(write)__asm__ volatile("at s1e0w, %1\nisb\nmrs %0, par_el1":"=r"(par):"r"((uint64_t)va):"memory");
    else __asm__ volatile("at s1e0r, %1\nisb\nmrs %0, par_el1":"=r"(par):"r"((uint64_t)va):"memory");
    return (par&1U)==0U;
}


int nova_mmu_user_verify_page(uintptr_t root_physical, uint16_t asid,
                              uintptr_t va, unsigned int flags) {
    uint64_t *l2, *l3, desc;
    size_t i2, i3;
    int writable = (flags & NOVA_MMU_MAP_WRITABLE) != 0U;
    int executable = (flags & NOVA_MMU_MAP_EXECUTABLE) != 0U;

    if (asid == 0U || asid > 255U ||
        (flags & ~(NOVA_MMU_MAP_WRITABLE | NOVA_MMU_MAP_EXECUTABLE)) != 0U ||
        (writable && executable) ||
        (va & (NOVA_MMU_PAGE_SIZE - 1U)) != 0U ||
        !user_va_indices(va, &i2, &i3))
        return 0;
    l2 = user_l2_for_root(root_physical);
    if (l2 == NULL ||
        (l2[i2] & (DESC_VALID | DESC_TABLE)) != (DESC_VALID | DESC_TABLE))
        return 0;
    l3 = (uint64_t *)(uintptr_t)(l2[i2] & DESC_ADDR_MASK);
    if (!nova_page_is_allocated(l3)) return 0;
    desc = l3[i3];
    if ((desc & (DESC_VALID | DESC_PAGE)) != (DESC_VALID | DESC_PAGE) ||
        (desc & DESC_AP_MASK) !=
            (writable ? DESC_AP_USER_RW : DESC_AP_USER_RO) ||
        (desc & DESC_PXN) == 0U ||
        descriptor_is_executable(desc) != executable)
        return 0;
    return 1;
}

int nova_mmu_user_protect_page(uintptr_t root_physical, uint16_t asid,
                               uintptr_t va, unsigned int flags) {
    uint64_t *l2, *l3, *slot, old_desc, new_desc;
    uintptr_t pa;
    size_t i2, i3, frame_index;
    int old_writable, old_executable;
    int new_writable = (flags & NOVA_MMU_MAP_WRITABLE) != 0U;
    int new_executable = (flags & NOVA_MMU_MAP_EXECUTABLE) != 0U;

    if (asid == 0U || asid > 255U ||
        (flags & ~(NOVA_MMU_MAP_WRITABLE | NOVA_MMU_MAP_EXECUTABLE)) != 0U ||
        (new_writable && new_executable) ||
        !user_va_indices(va, &i2, &i3))
        return 0;

    l2 = user_l2_for_root(root_physical);
    if (l2 == NULL ||
        (l2[i2] & (DESC_VALID | DESC_TABLE)) != (DESC_VALID | DESC_TABLE))
        return 0;
    l3 = (uint64_t *)(uintptr_t)(l2[i2] & DESC_ADDR_MASK);
    if (!nova_page_is_allocated(l3)) return 0;
    slot = &l3[i3];
    old_desc = *slot;
    if ((old_desc & (DESC_VALID | DESC_PAGE)) != (DESC_VALID | DESC_PAGE))
        return 0;

    pa = (uintptr_t)(old_desc & DESC_ADDR_MASK);
    if (pa < NOVA_MMU_RAM_BASE ||
        pa >= NOVA_MMU_RAM_BASE + NOVA_MMU_RAM_SIZE)
        return 0;
    frame_index = physical_frame_index(pa);
    old_writable = descriptor_is_writable(old_desc);
    old_executable = descriptor_is_executable(old_desc);
    if (!nova_page_is_allocated((const void *)pa) ||
        nova_page_pin_count((const void *)pa) == 0U ||
        mapped_aliases[frame_index] == 0U ||
        (old_writable && writable_aliases[frame_index] == 0U) ||
        (old_executable && executable_aliases[frame_index] == 0U))
        return 0;

    /* W^X is enforced across every alias, including other address spaces. */
    if ((new_writable &&
         executable_aliases[frame_index] > (size_t)old_executable) ||
        (new_executable &&
         writable_aliases[frame_index] > (size_t)old_writable) ||
        (!old_writable && new_writable &&
         writable_aliases[frame_index] == UINT32_MAX) ||
        (!old_executable && new_executable &&
         executable_aliases[frame_index] == UINT32_MAX))
        return 0;

    new_desc = old_desc & ~(DESC_AP_MASK | DESC_PXN | DESC_UXN);
    new_desc |= DESC_PXN; /* EL1 must never execute user-owned pages. */
    new_desc |= new_writable ? DESC_AP_USER_RW : DESC_AP_USER_RO;
    if (!new_executable) new_desc |= DESC_UXN;

    /*
     * Break-before-make: invalidate the old ASID+VA translation while the
     * descriptor is invalid, then publish the replacement descriptor.
     */
    *slot = 0U;
    invalidate_user_page(va, asid);
    *slot = new_desc;
    __asm__ volatile("dsb ishst\n isb" ::: "memory");

    if (old_writable && !new_writable) --writable_aliases[frame_index];
    if (!old_writable && new_writable) ++writable_aliases[frame_index];
    if (old_executable && !new_executable) --executable_aliases[frame_index];
    if (!old_executable && new_executable) ++executable_aliases[frame_index];
    return 1;
}
