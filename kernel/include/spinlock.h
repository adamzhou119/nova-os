#ifndef NOVA_SPINLOCK_H
#define NOVA_SPINLOCK_H

#include <stdint.h>

/*
 * IRQ-safe SMP spin lock. Acquire disables local IRQ delivery before spinning
 * so an interrupt handler on the owning CPU cannot recursively deadlock.
 * The lock is non-recursive; callers must define and obey a lock order.
 */
typedef struct nova_spinlock {
    uint32_t held;
} nova_spinlock_t;

#define NOVA_SPINLOCK_INITIALIZER { 0U }

uintptr_t nova_spin_lock_irqsave(nova_spinlock_t *lock);
void nova_spin_unlock_irqrestore(nova_spinlock_t *lock, uintptr_t saved_daif);

#endif
