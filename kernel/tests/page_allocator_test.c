#include "memory.h"

#include <stdio.h>
#include <stdint.h>

static int expect(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int overlaps(uintptr_t page, const struct nova_memory_region *range) {
    uintptr_t end = page + NOVA_PAGE_SIZE;
    uintptr_t range_end = (uintptr_t)(range->start + range->size);
    return page < range_end && (uintptr_t)range->start < end;
}

int main(void) {
    struct nova_memory_region ram = {0x1003U, 0x8ffdU};
    struct nova_memory_region reserved = {0x2800U, 0x1800U};
    struct nova_memory_region bad_memory = {UINT64_MAX - 0x1000U, 0x2000U};
    struct nova_memory_region overlap_memory[2] = {
        {0x1000U, 0x5000U}, {0x4000U, 0x3000U}
    };
    struct nova_memory_region bad_reserved = {UINT64_MAX - 0x10U, 0x20U};
    struct nova_memory_region too_many = {0x1000U, (32769ULL * NOVA_PAGE_SIZE)};
    void *pages[8];
    size_t i;

    if (!expect(nova_page_init_regions(&ram, 1U, &reserved, 1U),
                "unaligned RAM region should initialize")) return 1;
    if (!expect(nova_page_total() == 6U,
                "partial edge pages and reserved overlaps should be excluded")) return 1;
    if (!expect(nova_page_available() == 6U, "initial available count")) return 1;

    for (i = 0; i < 6U; ++i) {
        pages[i] = nova_page_alloc();
        if (!expect(pages[i] != NULL, "allocation within page capacity")) return 1;
        if (!expect(((uintptr_t)pages[i] & (NOVA_PAGE_SIZE - 1U)) == 0,
                    "allocated page must be aligned")) return 1;
        if (!expect(!overlaps((uintptr_t)pages[i], &reserved),
                    "allocated page must not overlap reserved range")) return 1;
    }
    if (!expect(nova_page_alloc() == NULL, "allocator must report exhaustion")) return 1;
    if (!expect(nova_page_available() == 0U, "available count at exhaustion")) return 1;
    if (!expect(nova_page_is_allocated(pages[0]), "allocated-page query recognizes owned page")) return 1;
    if (!expect(!nova_page_is_allocated((void *)0x2000U), "allocated-page query rejects foreign page")) return 1;
    if (!expect(nova_page_free(pages[0]), "valid allocated page can be freed")) return 1;
    if (!expect(!nova_page_is_allocated(pages[0]), "allocated-page query clears after free")) return 1;
    if (!expect(!nova_page_free(pages[0]), "double free must be rejected")) return 1;
    if (!expect(!nova_page_free((void *)0x2001U), "unaligned free must be rejected")) return 1;
    if (!expect(!nova_page_free((void *)0x90000000U), "foreign page must be rejected")) return 1;
    if (!expect(nova_page_alloc() == pages[0], "freed page should be reusable")) return 1;
    for (i = 1; i < 6U; ++i)
        if (!expect(nova_page_free(pages[i]), "remaining allocated page can be freed")) return 1;
    if (!expect(nova_page_free(pages[0]), "reused page can be freed")) return 1;
    if (!expect(nova_page_available() == nova_page_total(),
                "free count must recover after all pages are returned")) return 1;

    if (!expect(!nova_page_init_regions(NULL, 0U, NULL, 0U),
                "empty RAM descriptor list must fail")) return 1;
    if (!expect(nova_page_total() == 0U, "failed init must leave allocator disabled")) return 1;
    if (!expect(!nova_page_init_regions(&bad_memory, 1U, NULL, 0U),
                "overflowing RAM range must fail")) return 1;
    if (!expect(!nova_page_init_regions(overlap_memory, 2U, NULL, 0U),
                "overlapping RAM descriptors must fail")) return 1;
    if (!expect(!nova_page_init_regions(&ram, 1U, &bad_reserved, 1U),
                "overflowing reservation must fail even outside RAM")) return 1;
    if (!expect(!nova_page_init_regions(&too_many, 1U, NULL, 0U),
                "region exceeding static page index capacity must fail")) return 1;
    if (!expect(nova_page_total() == 0U, "capacity failure must not publish a partial map")) return 1;

    puts("Host page allocator edge-case tests passed.");
    return 0;
}
