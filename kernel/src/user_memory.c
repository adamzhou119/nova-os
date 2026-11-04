#include <stdint.h>
#include "mmu.h"
#include "user_memory.h"

#define NOVA_USER_PAGE_SIZE 4096U
#define NOVA_EFAULT (-14)
#define NOVA_EINVAL (-22)

int nova_user_check_range(uintptr_t address, uint64_t length, int write) {
    uintptr_t page;
    uintptr_t last_page;

    if (length == 0U) return 1;
    if (address < NOVA_USER_VA_BASE || address >= NOVA_USER_VA_END ||
        length > (uint64_t)(NOVA_USER_VA_END - address))
        return 0;

    page = address & ~(uintptr_t)(NOVA_USER_PAGE_SIZE - 1U);
    last_page = (address + (uintptr_t)length - 1U) &
                ~(uintptr_t)(NOVA_USER_PAGE_SIZE - 1U);
    for (;;) {
        if (!nova_mmu_verify_user_access(page, write)) return 0;
        if (page == last_page) break;
        page += NOVA_USER_PAGE_SIZE;
    }
    return 1;
}

int64_t nova_user_copy_from(void *kernel_destination,
                            uintptr_t user_source, uint64_t length) {
    uint64_t i;
    volatile const uint8_t *source =
        (volatile const uint8_t *)user_source;
    uint8_t *destination = (uint8_t *)kernel_destination;

    if (length > NOVA_USER_COPY_MAX) return NOVA_EINVAL;
    if (length == 0U) return 0;
    if (destination == 0 ||
        !nova_user_check_range(user_source, length, 0))
        return NOVA_EFAULT;

    /* Validate the complete range before the first user-memory load. */
    for (i = 0U; i < length; ++i) destination[i] = source[i];
    return (int64_t)length;
}

int64_t nova_user_copy_to(uintptr_t user_destination,
                          const void *kernel_source, uint64_t length) {
    uint64_t i;
    volatile uint8_t *destination =
        (volatile uint8_t *)user_destination;
    const uint8_t *source = (const uint8_t *)kernel_source;

    if (length > NOVA_USER_COPY_MAX) return NOVA_EINVAL;
    if (length == 0U) return 0;
    if (source == 0 ||
        !nova_user_check_range(user_destination, length, 1))
        return NOVA_EFAULT;

    /* Validate write permission on every covered page before storing. */
    for (i = 0U; i < length; ++i) destination[i] = source[i];
    return (int64_t)length;
}
