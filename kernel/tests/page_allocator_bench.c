#include <stdio.h>
#include <time.h>

#include "memory.h"

#define BENCH_MAX_PAGES 32768U
#define BENCH_REPETITIONS 12U
#define LOOKUP_REPETITIONS 8U
#define BENCH_BASE 0x10000000U

static void *pages[BENCH_MAX_PAGES];
static volatile size_t benchmark_checksum;

static double elapsed_us(clock_t start, clock_t end) {
    return 1000000.0 * (double)(end - start) / (double)CLOCKS_PER_SEC;
}

static int run_size(size_t count) {
    size_t repetition, i, lookup_round;
    clock_t alloc_ticks = 0, lookup_ticks = 0, free_ticks = 0;

    for (repetition = 0; repetition < BENCH_REPETITIONS; ++repetition) {
        clock_t start, end;

        if (!nova_page_init((uintptr_t)BENCH_BASE,
                            (uintptr_t)BENCH_BASE +
                                count * (size_t)NOVA_PAGE_SIZE)) {
            fprintf(stderr, "page allocator init failed for %zu pages\n", count);
            return 0;
        }

        start = clock();
        for (i = 0; i < count; ++i) {
            pages[i] = nova_page_alloc();
            if (pages[i] == NULL) {
                fprintf(stderr, "allocation failed at page %zu/%zu\n", i, count);
                return 0;
            }
        }
        end = clock();
        alloc_ticks += end - start;

        start = clock();
        for (lookup_round = 0; lookup_round < LOOKUP_REPETITIONS;
             ++lookup_round) {
            /* A deterministic permutation avoids repeatedly probing one cache line. */
            for (i = 0; i < count; ++i) {
                size_t index = (i * 4051U) % count;
                benchmark_checksum +=
                    (size_t)nova_page_is_allocated(pages[index]);
            }
        }
        end = clock();
        lookup_ticks += end - start;

        start = clock();
        for (i = count; i > 0; --i) {
            if (!nova_page_free(pages[i - 1U])) {
                fprintf(stderr, "free failed at page %zu/%zu\n", i - 1U, count);
                return 0;
            }
        }
        end = clock();
        free_ticks += end - start;

        if (nova_page_available() != nova_page_total()) {
            fprintf(stderr, "free-page accounting mismatch at %zu pages\n", count);
            return 0;
        }
    }

    {
        double alloc_ops = (double)count * BENCH_REPETITIONS;
        double lookup_ops = alloc_ops * LOOKUP_REPETITIONS;
        printf("%5zu pages | alloc %8.3f us/op | lookup %8.3f us/op"
               " | free %8.3f us/op\n",
               count,
               elapsed_us(0, alloc_ticks) / alloc_ops,
               elapsed_us(0, lookup_ticks) / lookup_ops,
               elapsed_us(0, free_ticks) / alloc_ops);
    }
    return 1;
}

int main(void) {
    static const size_t sizes[] = {256U, 4096U, BENCH_MAX_PAGES};
    size_t i;

    puts("Nova OS physical page allocator host benchmark");
    puts("CPU-time averages; informational only, not a CI pass/fail threshold.");
    puts("Pages | allocation | allocated-page lookup | free");
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        if (!run_size(sizes[i])) return 1;
    }
    printf("checksum: %zu\n", (size_t)benchmark_checksum);
    return 0;
}
