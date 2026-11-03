#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "task.h"
#include "vm.h"
#include "mmu.h"

#define BENCH_ITERATIONS 500000U

static _Alignas(16) unsigned char stack_pool[NOVA_TASK_MAX][4096];
static size_t next_stack;
static volatile uint32_t benchmark_checksum;

void nova_task_start_trampoline(void) {}

void *nova_vmap_alloc_guarded_page(unsigned int flags) {
    (void)flags;
    if (next_stack >= NOVA_TASK_MAX) return 0;
    return stack_pool[next_stack++];
}

int nova_vmap_free_guarded_page(void *address) {
    (void)address;
    return 1;
}

int nova_mmu_user_verify_page(uintptr_t root, uint16_t asid,
                              uintptr_t va, unsigned int flags) {
    (void)root;
    (void)asid;
    (void)va;
    (void)flags;
    return 0;
}

static void idle_task(void *argument) {
    (void)argument;
}

static int run_size(size_t task_count) {
    uint32_t ids[NOVA_TASK_MAX];
    uint32_t after_id = 0U;
    size_t i;
    clock_t start, end;
    double elapsed_us;

    for (i = 0; i < task_count; ++i) {
        if (!nova_task_create(idle_task, 0, &ids[i])) {
            fprintf(stderr, "task creation failed at %zu/%zu\n",
                    i + 1U, task_count);
            return 0;
        }
    }

    start = clock();
    for (i = 0; i < BENCH_ITERATIONS; ++i) {
        uint32_t selected = 0U;
        if (!nova_task_next_ready(after_id, &selected)) {
            fprintf(stderr, "ready-task selection failed at iteration %zu\n", i);
            return 0;
        }
        benchmark_checksum += selected;
        after_id = selected;
    }
    end = clock();

    elapsed_us = 1000000.0 * (double)(end - start) /
                 (double)CLOCKS_PER_SEC;
    printf("%2zu ready tasks | %8.3f us/selection | %8.1f selections/s\n",
           task_count, elapsed_us / (double)BENCH_ITERATIONS,
           (double)BENCH_ITERATIONS * 1000000.0 / elapsed_us);

    /* Keep each case isolated: all created tasks are still READY. */
    for (i = 0; i < task_count; ++i) {
        if (!nova_task_transition(ids[i], NOVA_TASK_RUNNING) ||
            !nova_task_transition(ids[i], NOVA_TASK_ZOMBIE) ||
            !nova_task_destroy(ids[i])) {
            fprintf(stderr, "task cleanup failed for id %u\n", ids[i]);
            return 0;
        }
    }
    return 1;
}

int main(void) {
    static const size_t task_counts[] = {1U, 4U, 8U};
    size_t i;

    puts("Nova OS ready-task selection host benchmark");
    puts("Measures task-ID round-robin candidate selection only.");
    puts("Does not measure context switching, IRQ latency, or device response.");
    puts("CPU-time averages; informational only, no CI timing threshold.");

    for (i = 0; i < sizeof(task_counts) / sizeof(task_counts[0]); ++i) {
        if (!run_size(task_counts[i])) return 1;
    }

    printf("checksum: %u\n", benchmark_checksum);
    return 0;
}
