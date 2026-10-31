#include "memory.h"

#define NOVA_PAGE_MAX_COUNT 1024U
#define NOVA_PAGE_WORD_BITS 64U
#define NOVA_PAGE_BITMAP_WORDS (NOVA_PAGE_MAX_COUNT / NOVA_PAGE_WORD_BITS)

static uint64_t page_bitmap[NOVA_PAGE_BITMAP_WORDS];
static uintptr_t page_base;
static size_t page_count;
static size_t page_free_count;
static int page_ready;

int nova_page_init(uintptr_t start, uintptr_t end) {
    size_t i;
    size_t count;

    if ((start & (NOVA_PAGE_SIZE - 1U)) != 0 ||
        (end & (NOVA_PAGE_SIZE - 1U)) != 0 || end <= start) {
        page_ready = 0;
        return 0;
    }

    count = (size_t)((end - start) / NOVA_PAGE_SIZE);
    if (count == 0 || count > NOVA_PAGE_MAX_COUNT) {
        page_ready = 0;
        return 0;
    }

    for (i = 0; i < NOVA_PAGE_BITMAP_WORDS; ++i) {
        page_bitmap[i] = 0;
    }

    page_base = start;
    page_count = count;
    page_free_count = count;
    page_ready = 1;
    return 1;
}

void *nova_page_alloc(void) {
    size_t index;

    if (!page_ready || page_free_count == 0) {
        return NULL;
    }

    for (index = 0; index < page_count; ++index) {
        size_t word = index / NOVA_PAGE_WORD_BITS;
        size_t bit = index % NOVA_PAGE_WORD_BITS;
        uint64_t mask = (uint64_t)1U << bit;

        if ((page_bitmap[word] & mask) == 0) {
            page_bitmap[word] |= mask;
            --page_free_count;
            return (void *)(page_base + index * NOVA_PAGE_SIZE);
        }
    }

    return NULL;
}

int nova_page_free(void *page) {
    uintptr_t address = (uintptr_t)page;
    uintptr_t offset;
    size_t index;
    size_t word;
    size_t bit;
    uint64_t mask;

    if (!page_ready || page == NULL || address < page_base) {
        return 0;
    }

    offset = address - page_base;
    if ((offset & (NOVA_PAGE_SIZE - 1U)) != 0 ||
        offset / NOVA_PAGE_SIZE >= page_count) {
        return 0;
    }

    index = (size_t)(offset / NOVA_PAGE_SIZE);
    word = index / NOVA_PAGE_WORD_BITS;
    bit = index % NOVA_PAGE_WORD_BITS;
    mask = (uint64_t)1U << bit;

    if ((page_bitmap[word] & mask) == 0) {
        return 0;
    }

    page_bitmap[word] &= ~mask;
    ++page_free_count;
    return 1;
}

size_t nova_page_total(void) {
    return page_ready ? page_count : 0;
}

size_t nova_page_available(void) {
    return page_ready ? page_free_count : 0;
}
