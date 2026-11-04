#include "memory.h"
#include "fdt.h"

#define NOVA_PAGE_MAX_COUNT 32768U
#define NOVA_PAGE_WORD_BITS 64U
#define NOVA_PAGE_BITMAP_WORDS (NOVA_PAGE_MAX_COUNT / NOVA_PAGE_WORD_BITS)

static uint64_t page_bitmap[NOVA_PAGE_BITMAP_WORDS];
static uintptr_t page_addresses[NOVA_PAGE_MAX_COUNT];
static uint32_t page_pin_counts[NOVA_PAGE_MAX_COUNT];
static size_t page_count;
static size_t page_free_count;
static int page_ready;
static size_t test_unpin_countdown;
static int test_unpin_failure_enabled;

static void page_reset(void) {
    size_t i;
    for (i = 0; i < NOVA_PAGE_BITMAP_WORDS; ++i) page_bitmap[i] = 0;
    for (i = 0; i < NOVA_PAGE_MAX_COUNT; ++i) page_pin_counts[i] = 0;
    page_count = 0;
    page_free_count = 0;
    page_ready = 0;
}

static int range_overlaps(uint64_t a_start, uint64_t a_end,
                          uint64_t b_start, uint64_t b_end) {
    return a_start < b_end && b_start < a_end;
}

int nova_page_init(uintptr_t start, uintptr_t end) {
    uintptr_t address;
    page_reset();
    if ((start & (NOVA_PAGE_SIZE - 1U)) != 0 ||
        (end & (NOVA_PAGE_SIZE - 1U)) != 0 || end <= start) return 0;
    if ((end - start) / NOVA_PAGE_SIZE > NOVA_PAGE_MAX_COUNT) return 0;
    for (address = start; address < end; address += NOVA_PAGE_SIZE)
        page_addresses[page_count++] = address;
    page_free_count = page_count;
    page_ready = page_count != 0;
    return page_ready;
}

/* Build a page list from firmware RAM, excluding every overlapping reservation. */
int nova_page_init_regions(const struct nova_memory_region *memory,
                           size_t memory_count,
                           const struct nova_memory_region *reserved,
                           size_t reserved_count) {
    size_t r, q;
    page_reset();
    if (memory == NULL || memory_count == 0 ||
        (reserved_count != 0 && reserved == NULL)) return 0;

    /* Reject malformed or overlapping RAM descriptors before building the map. */
    for (r = 0; r < memory_count; ++r) {
        uint64_t start = memory[r].start;
        uint64_t size = memory[r].size;
        if (size == 0 || start > UINT64_MAX - size) return 0;
        for (q = r + 1U; q < memory_count; ++q) {
            uint64_t other_start = memory[q].start;
            uint64_t other_size = memory[q].size;
            if (other_size == 0 || other_start > UINT64_MAX - other_size ||
                range_overlaps(start, start + size,
                               other_start, other_start + other_size)) return 0;
        }
    }

    /* Validate every reservation even when it lies outside discovered RAM. */
    for (q = 0; q < reserved_count; ++q) {
        if (reserved[q].size != 0 &&
            reserved[q].start > UINT64_MAX - reserved[q].size) return 0;
    }

    for (r = 0; r < memory_count; ++r) {
        uint64_t raw_start = memory[r].start;
        uint64_t raw_end = raw_start + memory[r].size;
        uint64_t address;
        if (raw_start > UINT64_MAX - (NOVA_PAGE_SIZE - 1U)) return 0;
        address = (raw_start + NOVA_PAGE_SIZE - 1U) &
                           ~((uint64_t)NOVA_PAGE_SIZE - 1U);
        uint64_t end = raw_end & ~((uint64_t)NOVA_PAGE_SIZE - 1U);
        while (address < end) {
            uint64_t page_end = address + NOVA_PAGE_SIZE;
            int blocked = 0;
            for (q = 0; q < reserved_count; ++q) {
                uint64_t reserve_start = reserved[q].start;
                uint64_t reserve_size = reserved[q].size;
                if (reserve_size == 0) continue;
                if (reserve_start > UINT64_MAX - reserve_size) return 0;
                if (range_overlaps(address, page_end, reserve_start,
                                   reserve_start + reserve_size)) {
                    blocked = 1;
                    break;
                }
            }
            if (!blocked) {
                if (page_count >= NOVA_PAGE_MAX_COUNT) return 0;
                page_addresses[page_count++] = (uintptr_t)address;
            }
            address += NOVA_PAGE_SIZE;
        }
    }
    page_free_count = page_count;
    page_ready = page_count != 0;
    return page_ready;
}

void *nova_page_alloc(void) {
    size_t index;
    if (!page_ready || page_free_count == 0) return NULL;
    for (index = 0; index < page_count; ++index) {
        size_t word = index / NOVA_PAGE_WORD_BITS;
        size_t bit = index % NOVA_PAGE_WORD_BITS;
        uint64_t mask = (uint64_t)1U << bit;
        if ((page_bitmap[word] & mask) == 0) {
            page_bitmap[word] |= mask;
            --page_free_count;
            return (void *)page_addresses[index];
        }
    }
    return NULL;
}

int nova_page_free(void *page) {
    uintptr_t address = (uintptr_t)page;
    size_t index, word, bit;
    uint64_t mask;
    if (!page_ready || page == NULL ||
        (address & (NOVA_PAGE_SIZE - 1U)) != 0) return 0;
    for (index = 0; index < page_count; ++index)
        if (page_addresses[index] == address) break;
    if (index == page_count) return 0;
    word = index / NOVA_PAGE_WORD_BITS;
    bit = index % NOVA_PAGE_WORD_BITS;
    mask = (uint64_t)1U << bit;
    if ((page_bitmap[word] & mask) == 0 || page_pin_counts[index] != 0) return 0;
    page_bitmap[word] &= ~mask;
    ++page_free_count;
    return 1;
}

size_t nova_page_total(void) { return page_ready ? page_count : 0; }
size_t nova_page_available(void) { return page_ready ? page_free_count : 0; }


int nova_page_is_allocated(const void *page) {
    uintptr_t address = (uintptr_t)page;
    size_t index, word, bit;
    uint64_t mask;
    if (!page_ready || page == NULL ||
        (address & (NOVA_PAGE_SIZE - 1U)) != 0) return 0;
    for (index = 0; index < page_count; ++index)
        if (page_addresses[index] == address) break;
    if (index == page_count) return 0;
    word = index / NOVA_PAGE_WORD_BITS;
    bit = index % NOVA_PAGE_WORD_BITS;
    mask = (uint64_t)1U << bit;
    return (page_bitmap[word] & mask) != 0;
}


static size_t page_find_index(const void *page) {
    size_t index;
    uintptr_t address = (uintptr_t)page;
    if (!page_ready || page == NULL ||
        (address & (NOVA_PAGE_SIZE - 1U)) != 0) return page_count;
    for (index = 0; index < page_count; ++index)
        if (page_addresses[index] == address) return index;
    return page_count;
}

int nova_page_pin(const void *page) {
    size_t index = page_find_index(page);
    size_t word, bit;
    uint64_t mask;
    if (index == page_count) return 0;
    word = index / NOVA_PAGE_WORD_BITS;
    bit = index % NOVA_PAGE_WORD_BITS;
    mask = (uint64_t)1U << bit;
    if ((page_bitmap[word] & mask) == 0 || page_pin_counts[index] == UINT32_MAX)
        return 0;
    ++page_pin_counts[index];
    return 1;
}

int nova_page_unpin(const void *page) {
    size_t index = page_find_index(page);
    if (index == page_count || page_pin_counts[index] == 0) return 0;
    if (test_unpin_failure_enabled) {
        if (test_unpin_countdown == 0U) {
            test_unpin_failure_enabled = 0;
            return 0;
        }
        --test_unpin_countdown;
    }
    --page_pin_counts[index];
    return 1;
}

void nova_page_test_fail_unpin_after(size_t successful_unpins) {
    test_unpin_countdown = successful_unpins;
    test_unpin_failure_enabled = 1;
}

void nova_page_test_clear_unpin_failure(void) {
    test_unpin_failure_enabled = 0;
    test_unpin_countdown = 0U;
}

size_t nova_page_pin_count(const void *page) {
    size_t index = page_find_index(page);
    return index == page_count ? 0U : (size_t)page_pin_counts[index];
}
