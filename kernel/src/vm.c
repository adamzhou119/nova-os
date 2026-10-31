#include "vm.h"

#include <stdint.h>
#include "memory.h"

#define NOVA_VM_VA_BASE 0x48000000ULL
#define NOVA_VM_VA_END  0x80000000ULL
#define NOVA_VM_PAGE_COUNT ((NOVA_VM_VA_END - NOVA_VM_VA_BASE) / NOVA_PAGE_SIZE)
#define NOVA_VM_BITMAP_WORDS ((NOVA_VM_PAGE_COUNT + 63U) / 64U)

static uint64_t virtual_page_bitmap[NOVA_VM_BITMAP_WORDS];

static int bitmap_test(size_t index) {
    return (virtual_page_bitmap[index / 64U] &
            (1ULL << (index % 64U))) != 0;
}

static void bitmap_set(size_t index) {
    virtual_page_bitmap[index / 64U] |= 1ULL << (index % 64U);
}

static void bitmap_clear(size_t index) {
    virtual_page_bitmap[index / 64U] &= ~(1ULL << (index % 64U));
}

void *nova_vmap_alloc_page(unsigned int flags) {
    size_t index;
    void *physical_page;
    uintptr_t virtual_address;

    for (index = 0; index < NOVA_VM_PAGE_COUNT; ++index)
        if (!bitmap_test(index)) break;
    if (index == NOVA_VM_PAGE_COUNT) return NULL;

    physical_page = nova_page_alloc();
    if (physical_page == NULL) return NULL;

    virtual_address = (uintptr_t)(NOVA_VM_VA_BASE +
                                  (uint64_t)index * NOVA_PAGE_SIZE);
    if (!nova_mmu_map_page(virtual_address, (uintptr_t)physical_page, flags)) {
        (void)nova_page_free(physical_page);
        return NULL;
    }

    bitmap_set(index);
    return (void *)virtual_address;
}

int nova_vmap_free_page(void *virtual_address) {
    uintptr_t address = (uintptr_t)virtual_address;
    uintptr_t physical_address;
    size_t index;

    if ((address & (NOVA_PAGE_SIZE - 1U)) != 0 ||
        address < NOVA_VM_VA_BASE || address >= NOVA_VM_VA_END) return 0;
    index = (size_t)((address - NOVA_VM_VA_BASE) / NOVA_PAGE_SIZE);
    if (!bitmap_test(index)) return 0;
    if (!nova_mmu_translate(address, &physical_address)) return 0;
    if (!nova_mmu_unmap_page(address)) return 0;

    bitmap_clear(index);
    return nova_page_free((void *)(physical_address &
                                   ~(uintptr_t)(NOVA_PAGE_SIZE - 1U)));
}
