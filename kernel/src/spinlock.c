#include "spinlock.h"

static uintptr_t irq_save_disable(void) {
#if defined(__aarch64__)
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2"
                     : "=r"(daif) :: "memory");
    return daif;
#else
    return 0U;
#endif
}

static void irq_restore(uintptr_t daif) {
#if defined(__aarch64__)
    __asm__ volatile("msr daif, %0" :: "r"(daif) : "memory");
#else
    (void)daif;
#endif
}

uintptr_t nova_spin_lock_irqsave(nova_spinlock_t *lock) {
    uintptr_t saved_daif = irq_save_disable();

    for (;;) {
        if (__atomic_exchange_n(&lock->held, 1U, __ATOMIC_ACQUIRE) == 0U)
            break;
        while (__atomic_load_n(&lock->held, __ATOMIC_RELAXED) != 0U) {
#if defined(__aarch64__)
            __asm__ volatile("wfe" ::: "memory");
#else
            __asm__ volatile("" ::: "memory");
#endif
        }
    }
    return saved_daif;
}

void nova_spin_unlock_irqrestore(nova_spinlock_t *lock, uintptr_t saved_daif) {
    __atomic_store_n(&lock->held, 0U, __ATOMIC_RELEASE);
#if defined(__aarch64__)
    __asm__ volatile("sev" ::: "memory");
#endif
    irq_restore(saved_daif);
}
