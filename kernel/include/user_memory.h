#ifndef NOVA_USER_MEMORY_H
#define NOVA_USER_MEMORY_H

#include <stdint.h>

/* Maximum bytes copied by one bounded user-memory operation. */
#define NOVA_USER_COPY_MAX 4096U

/*
 * Copy between the current EL0 address space and kernel-owned memory.
 * Return the byte count on success, -14 for an invalid/inaccessible user
 * range, or -22 when length exceeds NOVA_USER_COPY_MAX.
 */
int nova_user_validate_range(uintptr_t address, uint64_t length, int write_access);

int64_t nova_user_copy_from(void *kernel_destination,
                            uintptr_t user_source, uint64_t length);
int64_t nova_user_copy_to(uintptr_t user_destination,
                          const void *kernel_source, uint64_t length);

#endif
