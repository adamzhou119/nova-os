#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "memory.h"
#include "mmu.h"
#include "vm.h"

#define TEST_VA_BASE 0x48000000ULL
#define TEST_PAGE_SIZE 4096U
#define TEST_MAPPING_MAX 16U

static _Alignas(4096) unsigned char physical_pages[8][TEST_PAGE_SIZE];
static size_t next_physical_page;
static size_t freed_physical_pages;

struct test_mapping {
    uintptr_t virtual_address;
    uintptr_t physical_address;
    int used;
};

static struct test_mapping mappings[TEST_MAPPING_MAX];

void *nova_page_alloc(void) {
    if (next_physical_page >= 8U) return 0;
    return physical_pages[next_physical_page++];
}

int nova_page_free(void *address) {
    size_t i;
    for (i = 0; i < next_physical_page; ++i) {
        if (physical_pages[i] == (unsigned char *)address) {
            ++freed_physical_pages;
            return 1;
        }
    }
    return 0;
}

int nova_mmu_map_page(uintptr_t virtual_address, uintptr_t physical_address,
                      unsigned int flags) {
    size_t i;
    (void)flags;
    for (i = 0; i < TEST_MAPPING_MAX; ++i) {
        if (mappings[i].used &&
            mappings[i].virtual_address == virtual_address)
            return 0;
    }
    for (i = 0; i < TEST_MAPPING_MAX; ++i) {
        if (!mappings[i].used) {
            mappings[i].virtual_address = virtual_address;
            mappings[i].physical_address = physical_address;
            mappings[i].used = 1;
            return 1;
        }
    }
    return 0;
}

int nova_mmu_unmap_page(uintptr_t virtual_address) {
    size_t i;
    for (i = 0; i < TEST_MAPPING_MAX; ++i) {
        if (mappings[i].used &&
            mappings[i].virtual_address == virtual_address) {
            mappings[i].used = 0;
            return 1;
        }
    }
    return 0;
}

int nova_mmu_translate(uintptr_t virtual_address, uintptr_t *physical_address) {
    size_t i;
    uintptr_t page = virtual_address & ~(uintptr_t)(TEST_PAGE_SIZE - 1U);
    uintptr_t offset = virtual_address & (TEST_PAGE_SIZE - 1U);
    for (i = 0; i < TEST_MAPPING_MAX; ++i) {
        if (mappings[i].used && mappings[i].virtual_address == page) {
            if (physical_address != 0)
                *physical_address = mappings[i].physical_address + offset;
            return 1;
        }
    }
    return 0;
}

int main(void) {
    void *stack;
    void *ordinary;
    void *reused;
    uintptr_t translated = 0U;

    stack = nova_vmap_alloc_guarded_page(NOVA_MMU_MAP_WRITABLE);
    assert(stack == (void *)(TEST_VA_BASE + TEST_PAGE_SIZE));
    assert(!nova_mmu_translate(TEST_VA_BASE, &translated));
    assert(nova_mmu_translate((uintptr_t)stack, &translated));
    assert(!nova_vmap_free_page(stack));

    ordinary = nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE);
    assert(ordinary == (void *)(TEST_VA_BASE + 2U * TEST_PAGE_SIZE));
    assert(nova_mmu_translate((uintptr_t)ordinary, &translated));
    assert(!nova_mmu_translate(TEST_VA_BASE, &translated));

    assert(nova_vmap_free_guarded_page(stack));
    assert(!nova_mmu_translate((uintptr_t)stack, &translated));
    assert(!nova_mmu_translate(TEST_VA_BASE, &translated));
    assert(!nova_vmap_free_guarded_page(stack));

    /* Both slots of the guarded allocation are released for reuse. */
    reused = nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE);
    assert(reused == (void *)TEST_VA_BASE);
    assert(nova_vmap_free_page(ordinary));
    assert(nova_vmap_free_page(reused));
    assert(!nova_vmap_free_page(reused));
    assert(!nova_vmap_free_guarded_page((void *)TEST_VA_BASE));
    assert(!nova_vmap_free_guarded_page((void *)(TEST_VA_BASE + 1U)));

    assert(freed_physical_pages == 3U);
    puts("Kernel virtual guarded-stack allocator tests passed.");
    return 0;
}
