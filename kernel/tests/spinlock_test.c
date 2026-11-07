#include "spinlock.h"

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define THREAD_COUNT 8U
#define ITERATIONS 100000U

static nova_spinlock_t lock = NOVA_SPINLOCK_INITIALIZER;
static uint64_t protected_counter;

static void *worker(void *unused) {
    uint32_t i;
    (void)unused;
    for (i = 0U; i < ITERATIONS; ++i) {
        uintptr_t flags = nova_spin_lock_irqsave(&lock);
        ++protected_counter;
        nova_spin_unlock_irqrestore(&lock, flags);
    }
    return 0;
}

int main(void) {
    pthread_t threads[THREAD_COUNT];
    uint32_t i;
    for (i = 0U; i < THREAD_COUNT; ++i)
        assert(pthread_create(&threads[i], 0, worker, 0) == 0);
    for (i = 0U; i < THREAD_COUNT; ++i)
        assert(pthread_join(threads[i], 0) == 0);
    assert(protected_counter == (uint64_t)THREAD_COUNT * ITERATIONS);
    puts("SMP spinlock mutual-exclusion stress test passed.");
    return 0;
}
