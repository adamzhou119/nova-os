/*
 * Nova OS — minimal AArch64 kernel entry.
 * PL011 UART output for the QEMU virt experimental target.
 */
#include <stdint.h>
#include "memory.h"
#include "fdt.h"
#include "mmu.h"
#include "vm.h"
#include "task.h"
#include "context.h"
#include "scheduler.h"
#include "timer.h"
#include "gic.h"
#include "interrupt.h"
#include "exception.h"
#include "address_space.h"
#include "user_context.h"

static void uart_write(const char *s);
static void uart_write_hex64(uint64_t value);

#define UART0_BASE       0x09000000UL
#define UARTDR           (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UARTFR           (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UARTFR_TXFF      (1U << 5)

extern char __kernel_start[];
extern char __text_start[];
extern char __rodata_start[];
extern char __kernel_end[];
extern char __heap_start[];
extern char __heap_end[];
extern char __page_pool_start[];
extern char __page_pool_end[];

#ifdef NOVA_TEST_EL0_FAULT
extern const uint8_t nova_user_fault_payload_start[];
extern const uint8_t nova_user_fault_payload_end[];
extern const uint8_t nova_user_healthy_payload_start[];
extern const uint8_t nova_user_healthy_payload_end[];

static int test_el0_fault_containment(void) {
    uintptr_t root = 0U;
    uintptr_t fault_code = 0U, fault_stack = 0U;
    uintptr_t healthy_code = 0U, healthy_stack = 0U;
    uint32_t space_id = 0U, fault_task = 0U, healthy_task = 0U;
    uintptr_t fault_size = (uintptr_t)(nova_user_fault_payload_end -
                                       nova_user_fault_payload_start);
    uintptr_t healthy_size = (uintptr_t)(nova_user_healthy_payload_end -
                                         nova_user_healthy_payload_start);
    volatile uint64_t *healthy_result;
    size_t i;
    int ok = 0;

    if (fault_size == 0U || fault_size > NOVA_PAGE_SIZE ||
        healthy_size == 0U || healthy_size > NOVA_PAGE_SIZE)
        return 0;
    fault_code = (uintptr_t)nova_page_alloc();
    fault_stack = (uintptr_t)nova_page_alloc();
    healthy_code = (uintptr_t)nova_page_alloc();
    healthy_stack = (uintptr_t)nova_page_alloc();
    if (fault_code == 0U || fault_stack == 0U ||
        healthy_code == 0U || healthy_stack == 0U ||
        !nova_mmu_address_space_create(&root) ||
        !nova_address_space_create(root, 32U, &space_id))
        goto cleanup;

    for (i = 0; i < NOVA_PAGE_SIZE; ++i) {
        ((volatile uint8_t *)fault_code)[i] = 0U;
        ((volatile uint8_t *)fault_stack)[i] = 0U;
        ((volatile uint8_t *)healthy_code)[i] = 0U;
        ((volatile uint8_t *)healthy_stack)[i] = 0U;
    }
    for (i = 0; i < fault_size; ++i)
        ((volatile uint8_t *)fault_code)[i] = nova_user_fault_payload_start[i];
    for (i = 0; i < healthy_size; ++i)
        ((volatile uint8_t *)healthy_code)[i] = nova_user_healthy_payload_start[i];

    if (!nova_mmu_user_map_page(root, 32U, NOVA_USER_VA_BASE + NOVA_PAGE_SIZE,
                                fault_code, NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_user_map_page(root, 32U, NOVA_USER_VA_BASE + 2U * NOVA_PAGE_SIZE,
                                fault_stack, NOVA_MMU_MAP_WRITABLE) ||
        !nova_mmu_user_map_page(root, 32U, NOVA_USER_VA_BASE + 4U * NOVA_PAGE_SIZE,
                                healthy_code, NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_user_map_page(root, 32U, NOVA_USER_VA_BASE + 5U * NOVA_PAGE_SIZE,
                                healthy_stack, NOVA_MMU_MAP_WRITABLE) ||
        !nova_task_create_user(space_id, NOVA_USER_VA_BASE + NOVA_PAGE_SIZE,
                               NOVA_USER_VA_BASE + 3U * NOVA_PAGE_SIZE,
                               0U, &fault_task) ||
        !nova_task_create_user(space_id, NOVA_USER_VA_BASE + 3U * NOVA_PAGE_SIZE,
                               NOVA_USER_VA_BASE + 5U * NOVA_PAGE_SIZE,
                               0U, &healthy_task))
        goto cleanup;

    healthy_result = (volatile uint64_t *)(healthy_stack + NOVA_PAGE_SIZE -
                                           2U * sizeof(uint64_t));
    uart_write("EL0 fault test task prepared; task id: ");
    uart_write_hex64(fault_task);
    uart_write("EL0 healthy peer prepared; task id: ");
    uart_write_hex64(healthy_task);
    if (!nova_sched_start()) {
        uart_write("EL0 fault test scheduler returned failure.\n");
        goto cleanup;
    }

    uart_write("EL0 healthy peer marker: ");
    uart_write_hex64(*healthy_result);
    ok = *healthy_result == healthy_task &&
         !nova_task_get(fault_task, &(struct nova_task_info){0}) &&
         !nova_task_get(healthy_task, &(struct nova_task_info){0});
    if (ok)
        uart_write("EL0 fault task reaped; healthy peer completed.\n");

cleanup:
    (void)nova_mmu_switch_address_space(nova_mmu_kernel_root(), 0U);
    if (fault_task != 0U) {
        struct nova_task_info info;
        if (nova_task_get(fault_task, &info) &&
            (info.state == NOVA_TASK_READY || info.state == NOVA_TASK_ZOMBIE))
            (void)nova_task_destroy(fault_task);
    }
    if (healthy_task != 0U) {
        struct nova_task_info info;
        if (nova_task_get(healthy_task, &info) &&
            (info.state == NOVA_TASK_READY || info.state == NOVA_TASK_ZOMBIE))
            (void)nova_task_destroy(healthy_task);
    }
    if (space_id != 0U) (void)nova_address_space_release(space_id);
    if (root != 0U) {
        (void)nova_mmu_user_unmap_page(root, 32U, NOVA_USER_VA_BASE + NOVA_PAGE_SIZE);
        (void)nova_mmu_user_unmap_page(root, 32U, NOVA_USER_VA_BASE + 2U * NOVA_PAGE_SIZE);
        (void)nova_mmu_user_unmap_page(root, 32U, NOVA_USER_VA_BASE + 3U * NOVA_PAGE_SIZE);
        (void)nova_mmu_user_unmap_page(root, 32U, NOVA_USER_VA_BASE + 5U * NOVA_PAGE_SIZE);
        (void)nova_mmu_address_space_destroy(root);
    }
    if (fault_code != 0U) (void)nova_page_free((void *)fault_code);
    if (fault_stack != 0U) (void)nova_page_free((void *)fault_stack);
    if (healthy_code != 0U) (void)nova_page_free((void *)healthy_code);
    if (healthy_stack != 0U) (void)nova_page_free((void *)healthy_stack);
    return ok;
}
#endif

#ifdef NOVA_TEST_EL0_SVC
extern const uint8_t nova_user_test_payload_start[];
extern const uint8_t nova_user_test_payload_end[];

static int test_el0_svc_round_trip(void) {
    uintptr_t root = 0U;
    uintptr_t code_page = 0U, stack_page = 0U;
    uint32_t space_id = 0U, task_id = 0U;
    uintptr_t code_size = (uintptr_t)(nova_user_test_payload_end -
                                      nova_user_test_payload_start);
    volatile uint64_t *result;
    size_t i;
    int ok = 0;

    if (code_size == 0U || code_size > NOVA_PAGE_SIZE) return 0;
    code_page = (uintptr_t)nova_page_alloc();
    stack_page = (uintptr_t)nova_page_alloc();
    if (code_page == 0U || stack_page == 0U ||
        !nova_mmu_address_space_create(&root) ||
        !nova_address_space_create(root, 31U, &space_id))
        goto cleanup;

    for (i = 0; i < NOVA_PAGE_SIZE; ++i) {
        ((volatile uint8_t *)code_page)[i] = 0U;
        ((volatile uint8_t *)stack_page)[i] = 0U;
    }
    for (i = 0; i < code_size; ++i)
        ((volatile uint8_t *)code_page)[i] = nova_user_test_payload_start[i];

    if (!nova_mmu_user_map_page(root, 31U, NOVA_USER_VA_BASE + NOVA_PAGE_SIZE,
                                code_page, NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_user_map_page(root, 31U, NOVA_USER_VA_BASE + 2U * NOVA_PAGE_SIZE,
                                stack_page, NOVA_MMU_MAP_WRITABLE) ||
        !nova_task_create_user(space_id, NOVA_USER_VA_BASE + NOVA_PAGE_SIZE,
                               NOVA_USER_VA_BASE + 3U * NOVA_PAGE_SIZE,
                               0U, &task_id))
        goto cleanup;

    result = (volatile uint64_t *)(stack_page + NOVA_PAGE_SIZE -
                                      2U * sizeof(uint64_t));
    uart_write("EL0 test task prepared; task id: ");
    uart_write_hex64(task_id);
    if (!nova_sched_start()) {
        uart_write("EL0 test scheduler returned failure.\n");
        goto cleanup;
    }
    uart_write("EL0 test scheduler returned; stack result: ");
    uart_write_hex64(*result);
    uart_write("EL0 test expected task id: ");
    uart_write_hex64(task_id);
    ok = *result == task_id;

cleanup:
    (void)nova_mmu_switch_address_space(nova_mmu_kernel_root(), 0U);
    if (task_id != 0U) {
        struct nova_task_info info;
        if (nova_task_get(task_id, &info) &&
            info.state == NOVA_TASK_READY)
            (void)nova_task_destroy(task_id);
    }
    if (space_id != 0U) {
        (void)nova_address_space_release(space_id);
    }
    if (root != 0U) {
        (void)nova_mmu_user_unmap_page(root, 31U, NOVA_USER_VA_BASE + NOVA_PAGE_SIZE);
        (void)nova_mmu_user_unmap_page(root, 31U, NOVA_USER_VA_BASE + 2U * NOVA_PAGE_SIZE);
        (void)nova_mmu_address_space_destroy(root);
    }
    if (code_page != 0U) (void)nova_page_free((void *)code_page);
    if (stack_page != 0U) (void)nova_page_free((void *)stack_page);
    return ok;
}
#endif

static volatile uint64_t bss_probe;
static struct nova_memory_region detected_memory[NOVA_FDT_MAX_MEMORY_REGIONS];
static struct nova_memory_region reserved_memory[NOVA_FDT_MAX_RESERVED_REGIONS];
static struct nova_memory_region allocator_reserved[NOVA_FDT_MAX_RESERVED_REGIONS + 2U];

static void uart_putc(char c) {
    while (UARTFR & UARTFR_TXFF) __asm__ volatile("nop");
    UARTDR = (uint32_t)c;
}

static void uart_write(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    uart_write("0x");
    for (shift = 60; shift >= 0; shift -= 4)
        uart_putc(digits[(value >> shift) & 0x0fU]);
    uart_putc('\n');
}

static void uart_write_current_el(uint64_t current_el) {
    uart_write("CurrentEL: EL");
    uart_putc((char)('0' + (current_el & 0x3U)));
    uart_putc('\n');
}

static const char *exception_vector_name(uint64_t vector) {
    switch (vector) {
    case 0: return "Current EL SP0 synchronous";
    case 1: return "Current EL SP0 IRQ";
    case 2: return "Current EL SP0 FIQ";
    case 3: return "Current EL SP0 SError";
    case 4: return "Current EL SPx synchronous";
    case 5: return "Current EL SPx IRQ";
    case 6: return "Current EL SPx FIQ";
    case 7: return "Current EL SPx SError";
    case 8: return "Lower EL AArch64 synchronous";
    case 9: return "Lower EL AArch64 IRQ";
    case 10: return "Lower EL AArch64 FIQ";
    case 11: return "Lower EL AArch64 SError";
    case 12: return "Lower EL AArch32 synchronous";
    case 13: return "Lower EL AArch32 IRQ";
    case 14: return "Lower EL AArch32 FIQ";
    case 15: return "Lower EL AArch32 SError";
    default: return "Unknown vector";
    }
}

static void exception_report_at_level(uint64_t level, uint64_t esr,
                                       uint64_t elr, uint64_t vector,
                                       uint64_t far, uint64_t spsr) {
    uart_write("EXCEPTION: EL");
    uart_putc((char)('0' + level));
    uart_write(" vector captured.\n");
    uart_write("Exception kind: ");
    uart_write(nova_exception_kind(vector));
    uart_putc('\n');
    uart_write("Vector: ");
    uart_write(exception_vector_name(vector));
    uart_putc('\n');
    uart_write("Vector slot: ");
    uart_write_hex64(vector);
    uart_write("ESR_EL");
    uart_putc((char)('0' + level));
    uart_write(": ");
    uart_write_hex64(esr);
    uart_write("ESR exception class: ");
    uart_write_hex64((esr >> 26) & 0x3fU);
    uart_write("ESR exception description: ");
    uart_write(nova_exception_class_name(esr));
    uart_putc('\n');
    uart_write("ESR instruction-specific syndrome: ");
    uart_write_hex64(esr & 0x01ffffffU);
    if (nova_exception_is_instruction_abort(esr) ||
        nova_exception_is_data_abort(esr)) {
        uart_write("Abort fault status: ");
        uart_write(nova_abort_fault_status_name(esr));
        uart_putc('\n');
        uart_write("Fault address valid: ");
        uart_write(nova_exception_fault_address_valid(esr) ? "yes\n" : "no\n");
        uart_write("Stage-1 page-table walk: ");
        uart_write(nova_exception_is_stage1_page_walk(esr) ? "yes\n" : "no\n");
        if (nova_exception_is_data_abort(esr)) {
            uart_write("Data access: ");
            uart_write(nova_exception_is_write_abort(esr) ? "write\n" : "read\n");
        }
    }
    uart_write("ELR_EL");
    uart_putc((char)('0' + level));
    uart_write(": ");
    uart_write_hex64(elr);
    uart_write("FAR_EL");
    uart_putc((char)('0' + level));
    uart_write(": ");
    uart_write_hex64(far);
    uart_write("SPSR_EL");
    uart_putc((char)('0' + level));
    uart_write(": ");
    uart_write_hex64(spsr);
    uart_write("System halted for diagnosis.\n");
    for (;;) __asm__ volatile("wfe");

}

void exception_report(uint64_t esr, uint64_t elr, uint64_t vector,
                      uint64_t far, uint64_t spsr) {
    exception_report_at_level(1U, esr, elr, vector, far, spsr);
}

void exception_report_el2(uint64_t esr, uint64_t elr, uint64_t vector,
                          uint64_t far, uint64_t spsr) {
    exception_report_at_level(2U, esr, elr, vector, far, spsr);
}

/*
 * Minimal EL0 SVC ABI: x8 selects the syscall; x0 is the return value.
 * Syscall 0 returns the current task ID. Unknown calls return -ENOSYS (-38).
 * Other synchronous exceptions remain terminal until fault isolation exists.
 */
/* Bounded UART write: validate every covered user page before dereferencing. */
static int64_t nova_user_write(uintptr_t user_buffer, uint64_t length) {
    uintptr_t page, last_page;
    uint64_t i;

    if (length > 128U) return -22; /* -EINVAL */
    if (length == 0U) return 0;
    if (user_buffer < NOVA_USER_VA_BASE ||
        user_buffer >= NOVA_USER_VA_END ||
        length > (uint64_t)(NOVA_USER_VA_END - user_buffer))
        return -14; /* -EFAULT */

    page = user_buffer & ~(uintptr_t)(NOVA_PAGE_SIZE - 1U);
    last_page = (user_buffer + (uintptr_t)length - 1U) &
                ~(uintptr_t)(NOVA_PAGE_SIZE - 1U);
    for (;;) {
        if (!nova_mmu_verify_user_access(page, 0))
            return -14;
        if (page == last_page) break;
        page += NOVA_PAGE_SIZE;
    }

    uart_write("EL0 user message: ");
    for (i = 0U; i < length; ++i)
        uart_putc(((volatile const char *)user_buffer)[i]);
    return (int64_t)length;
}

uintptr_t nova_user_sync_dispatch(uintptr_t frame_address) {
    struct nova_user_trap_frame *frame =
        (struct nova_user_trap_frame *)frame_address;
    enum nova_exception_policy policy = nova_exception_policy_for(
        8U, frame->esr_el1, frame->spsr_el1);
    uint32_t exception_class = (uint32_t)((frame->esr_el1 >> 26) & 0x3fU);

    if (policy == NOVA_EXCEPTION_POLICY_KERNEL_FATAL) {
        exception_report(frame->esr_el1, frame->elr_el1, 8U,
                         frame->far_el1, frame->spsr_el1);
        for (;;) __asm__ volatile("wfe");
    }

    if (policy == NOVA_EXCEPTION_POLICY_USER_FAULT) {
        uint32_t task_id = nova_sched_current();
        enum nova_task_fault_kind kind;
        switch (exception_class) {
        case 0x20U: kind = NOVA_TASK_FAULT_INSTRUCTION_ABORT; break;
        case 0x24U: kind = NOVA_TASK_FAULT_DATA_ABORT; break;
        case 0x30U: kind = NOVA_TASK_FAULT_BREAKPOINT; break;
        case 0x32U: kind = NOVA_TASK_FAULT_SOFTWARE_STEP; break;
        case 0x34U: kind = NOVA_TASK_FAULT_WATCHPOINT; break;
        case 0x3cU: kind = NOVA_TASK_FAULT_BRK; break;
        default:
            exception_report(frame->esr_el1, frame->elr_el1, 8U,
                             frame->far_el1, frame->spsr_el1);
            for (;;) __asm__ volatile("wfe");
        }
        if (task_id == 0U ||
            !nova_task_set_fault(task_id, kind, frame->esr_el1,
                                 frame->far_el1, frame->elr_el1)) {
            exception_report(frame->esr_el1, frame->elr_el1, 8U,
                             frame->far_el1, frame->spsr_el1);
            for (;;) __asm__ volatile("wfe");
        }
        uart_write("EL0 fault contained; task id: ");
        uart_write_hex64(task_id);
        uart_write("ESR exception description: ");
        uart_write(nova_exception_class_name(frame->esr_el1));
        uart_write("\n");
        uart_write("EL0 fault ESR: ");
        uart_write_hex64(frame->esr_el1);
        uart_write("EL0 fault FAR: ");
        uart_write_hex64(frame->far_el1);
        uart_write("EL0 fault ELR: ");
        uart_write_hex64(frame->elr_el1);
        nova_sched_task_exit();
        for (;;) __asm__ volatile("wfe");
    }

    /* Remaining eligible lower-EL synchronous traps are SVC calls. */
    if (frame->x[8] == 0U) {
#ifdef NOVA_TEST_EL0_SVC
        uart_write("EL0 SVC get-task-id; current task: ");
        uart_write_hex64(nova_sched_current());
#endif
        frame->x[0] = (uint64_t)nova_sched_current();
    } else if (frame->x[8] == 2U) {
        int64_t written = nova_user_write((uintptr_t)frame->x[0],
                                          frame->x[1]);
#ifdef NOVA_TEST_EL0_SVC
        if (written == -14)
            uart_write("EL0 write syscall rejected invalid pointer.\n");
        if (written == -22)
            uart_write("EL0 write syscall rejected excessive length.\n");
#endif
        frame->x[0] = (uint64_t)written;
    } else if (frame->x[8] == 1U) {
#ifdef NOVA_TEST_EL0_SVC
        uart_write("EL0 SVC exit; current task: ");
        uart_write_hex64(nova_sched_current());
        uart_write("EL0 SVC exit SP_EL0: ");
        uart_write_hex64(frame->sp_el0);
#endif
        uint32_t task_id = nova_sched_current();
        (void)nova_task_set_exit_code(task_id, (int)(int64_t)frame->x[0]);
        nova_sched_task_exit();
    } else {
        frame->x[0] = (uint64_t)(int64_t)-38;
    }
    /* AArch64 SVC records the address of the following instruction in ELR_EL1. */
    return frame_address;
}

static int test_heap(void) {
    void *small;
    void *page;
    void *too_large;

    if (!nova_heap_init((uintptr_t)__heap_start, (uintptr_t)__heap_end))
        return 0;

    small = nova_alloc(37, 16);
    page = nova_alloc(4096, 4096);
    too_large = nova_alloc(0x100000, 16);

    return small != NULL && page != NULL &&
           (((uintptr_t)small & 15U) == 0) &&
           (((uintptr_t)page & 4095U) == 0) &&
           too_large == NULL && nova_heap_used() <= 0x100000U;
}

static int test_pages(void) {
    void *first;
    void *second;
    void *reused;
    size_t initial;

    if (!nova_page_init((uintptr_t)__page_pool_start,
                        (uintptr_t)__page_pool_end)) {
        uart_write("Page test: initialization failed.\n");
        return 0;
    }

    initial = nova_page_total();
    first = nova_page_alloc();
    second = nova_page_alloc();
    if (first == NULL || second == NULL) {
        uart_write("Page test: allocation returned NULL.\n");
        return 0;
    }
    if (first == second) {
        uart_write("Page test: duplicate allocation address.\n");
        return 0;
    }
    if ((((uintptr_t)first & (NOVA_PAGE_SIZE - 1U)) != 0) ||
        (((uintptr_t)second & (NOVA_PAGE_SIZE - 1U)) != 0)) {
        uart_write("Page test: returned address is not page aligned.\n");
        return 0;
    }

    if (!nova_page_free(first)) {
        uart_write("Page test: first free failed.\n");
        return 0;
    }
    if (nova_page_free(first)) {
        uart_write("Page test: double free was accepted.\n");
        return 0;
    }
    if (nova_page_free((void *)((uintptr_t)second + 1U))) {
        uart_write("Page test: unaligned free was accepted.\n");
        return 0;
    }

    reused = nova_page_alloc();
    if (reused != first || nova_page_available() != initial - 2U) {
        uart_write("Page test: freed page was not reused correctly.\n");
        return 0;
    }

    if (!nova_page_free(second) || !nova_page_free(reused)) {
        uart_write("Page test: final free failed.\n");
        return 0;
    }

    if (nova_page_available() != initial) {
        uart_write("Page test: free-page count did not recover.\n");
        return 0;
    }
    uart_write("Page allocator checks passed individually.\n");
    return 1;
}


static int range_contains(uint64_t start, uint64_t size, uint64_t address) {
    return size != 0 && start <= UINT64_MAX - size &&
           address >= start && address < start + size;
}

static int test_firmware_pages(int memory_regions, int reserved_regions,
                               uintptr_t dtb_address) {
    size_t i;
    size_t initial;
    size_t reserve_count = 0;
    size_t dtb_size = nova_fdt_total_size(dtb_address);
    void *pages[3];

    if (memory_regions <= 0 || reserved_regions < 0 || dtb_size == 0 ||
        dtb_address > UINTPTR_MAX - dtb_size) return 0;

    for (i = 0; i < (size_t)reserved_regions; ++i)
        allocator_reserved[reserve_count++] = reserved_memory[i];

    allocator_reserved[reserve_count].start = (uint64_t)(uintptr_t)__kernel_start;
    allocator_reserved[reserve_count++].size =
        (uint64_t)((uintptr_t)__kernel_end - (uintptr_t)__kernel_start);
    allocator_reserved[reserve_count].start = (uint64_t)dtb_address;
    allocator_reserved[reserve_count++].size = (uint64_t)dtb_size;

    if (!nova_page_init_regions(detected_memory, (size_t)memory_regions,
                                allocator_reserved, reserve_count)) {
        uart_write("Firmware page test: map initialization failed.\n");
        return 0;
    }

    initial = nova_page_total();
    if (initial == 0) return 0;
    for (i = 0; i < 3U; ++i) {
        size_t r;
        uintptr_t address;
        pages[i] = nova_page_alloc();
        if (pages[i] == NULL) return 0;
        address = (uintptr_t)pages[i];
        if ((address & (NOVA_PAGE_SIZE - 1U)) != 0) return 0;
        for (r = 0; r < reserve_count; ++r) {
            uint64_t reserve_start = allocator_reserved[r].start;
            uint64_t reserve_size = allocator_reserved[r].size;
            if (range_contains(reserve_start, reserve_size, address) ||
                (reserve_size != 0 && reserve_start <= UINT64_MAX - reserve_size &&
                 address < reserve_start &&
                 address + NOVA_PAGE_SIZE > reserve_start)) {
                uart_write("Firmware page test: reserved page was allocated.\n");
                return 0;
            }
        }
    }

    for (i = 0; i < 3U; ++i)
        if (!nova_page_free(pages[i])) return 0;
    if (nova_page_available() != initial) return 0;
    uart_write("Firmware RAM page allocator tests passed.\n");
    return 1;
}


static int test_dynamic_virtual_pages(void) {
    const uintptr_t alias = 0x48000000U;
    const uintptr_t second_alias = 0x48001000U;
    const uint64_t initial_value = 0x4E4F56414D4D5531ULL;
    const uint64_t alias_value = 0x504147454D415031ULL;
    void *physical_page = nova_page_alloc();
    volatile uint64_t *physical_word;
    volatile uint64_t *alias_word;
    uintptr_t physical_address;

    if (physical_page == NULL) return 0;
    physical_address = (uintptr_t)physical_page;
    if ((physical_address & (NOVA_PAGE_SIZE - 1U)) != 0) {
        nova_page_free(physical_page);
        return 0;
    }

    physical_word = (volatile uint64_t *)physical_address;
    *physical_word = initial_value;

    /* Reject malformed mappings and enforce per-mapping write-xor-execute. */
    if (nova_mmu_map_page(alias + 1U, physical_address, 0U) ||
        nova_mmu_map_page(alias, physical_address + 1U, 0U) ||
        nova_mmu_map_page(alias, 0x48000000U, NOVA_MMU_MAP_WRITABLE) ||
        nova_mmu_map_page(alias, physical_address,
                          NOVA_MMU_MAP_WRITABLE | NOVA_MMU_MAP_EXECUTABLE) ||
        nova_mmu_protect_page(alias, NOVA_MMU_MAP_WRITABLE)) {
        (void)nova_page_free(physical_page);
        return 0;
    }

    /* First map read-only and prove the write permission is denied. */
    if (!nova_mmu_map_page(alias, physical_address, 0U) ||
        !nova_mmu_verify_translation(alias, physical_address) ||
        nova_page_pin_count(physical_page) != 1U ||
        nova_page_free(physical_page) ||
        !nova_mmu_verify_write_protected(alias) ||
        !nova_mmu_verify_execute_never(alias) ||
        nova_mmu_map_page(alias, physical_address, 0U)) {
        (void)nova_mmu_unmap_page(alias);
        (void)nova_page_free(physical_page);
        return 0;
    }

    /*
     * Global W^X: a second executable alias must block writable permission
     * on the first alias, and vice versa.
     */
    if (!nova_mmu_map_page(second_alias, physical_address,
                           NOVA_MMU_MAP_EXECUTABLE) ||
        nova_page_pin_count(physical_page) != 2U ||
        nova_mmu_protect_page(alias, NOVA_MMU_MAP_WRITABLE) ||
        !nova_mmu_unmap_page(second_alias) ||
        nova_page_pin_count(physical_page) != 1U ||
        !nova_mmu_protect_page(alias, NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_verify_write_protected(alias) ||
        !nova_mmu_verify_executable(alias) ||
        nova_mmu_map_page(second_alias, physical_address,
                          NOVA_MMU_MAP_WRITABLE) ||
        nova_mmu_protect_page(alias, NOVA_MMU_MAP_WRITABLE |
                                    NOVA_MMU_MAP_EXECUTABLE) ||
        !nova_mmu_protect_page(alias, NOVA_MMU_MAP_WRITABLE) ||
        !nova_mmu_verify_writable(alias) ||
        !nova_mmu_verify_execute_never(alias)) {
        (void)nova_mmu_unmap_page(second_alias);
        (void)nova_mmu_unmap_page(alias);
        (void)nova_page_free(physical_page);
        return 0;
    }

    if (!nova_mmu_unmap_page(alias) ||
        nova_page_pin_count(physical_page) != 0U ||
        !nova_mmu_verify_unmapped(alias) ||
        nova_mmu_unmap_page(alias)) {
        (void)nova_mmu_unmap_page(alias);
        (void)nova_page_free(physical_page);
        return 0;
    }

    /* Exercise the final page in the reserved virtual window and its guard. */
    if (nova_mmu_map_page(0x80000000U, physical_address,
                          NOVA_MMU_MAP_WRITABLE) ||
        nova_mmu_map_page(0x47fff000U, physical_address,
                          NOVA_MMU_MAP_WRITABLE) ||
        !nova_mmu_map_page(0x7ffff000U, physical_address,
                           NOVA_MMU_MAP_WRITABLE) ||
        !nova_mmu_verify_translation(0x7ffff000U, physical_address) ||
        !nova_mmu_unmap_page(0x7ffff000U) ||
        !nova_mmu_verify_unmapped(0x7ffff000U)) {
        (void)nova_mmu_unmap_page(0x7ffff000U);
        (void)nova_page_free(physical_page);
        return 0;
    }

    /* Re-map writable and verify alias reads/writes hit the same RAM page. */
    if (!nova_mmu_map_page(alias, physical_address, NOVA_MMU_MAP_WRITABLE) ||
        !nova_mmu_verify_translation(alias, physical_address) ||
        !nova_mmu_verify_writable(alias)) {
        (void)nova_mmu_unmap_page(alias);
        (void)nova_page_free(physical_page);
        return 0;
    }
    alias_word = (volatile uint64_t *)alias;
    if (*alias_word != initial_value) {
        (void)nova_mmu_unmap_page(alias);
        (void)nova_page_free(physical_page);
        return 0;
    }
    *alias_word = alias_value;
    if (*physical_word != alias_value) {
        (void)nova_mmu_unmap_page(alias);
        (void)nova_page_free(physical_page);
        return 0;
    }

    if (!nova_mmu_unmap_page(alias) || !nova_mmu_verify_unmapped(alias) ||
        !nova_page_free(physical_page)) return 0;
    return 1;
}


static int test_virtual_page_allocator(void) {
    const uint64_t first_value = 0x564D415050414745ULL;
    const uint64_t second_value = 0x564D5345434F4E44ULL;
    void *first = nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE);
    void *second = nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE);
    void *reused;
    uintptr_t physical_address;
    uintptr_t first_physical_address;

    if (first == NULL || second == NULL || first == second) {
        if (first != NULL) (void)nova_vmap_free_page(first);
        if (second != NULL) (void)nova_vmap_free_page(second);
        return 0;
    }

    *(volatile uint64_t *)first = first_value;
    *(volatile uint64_t *)second = second_value;
    if (*(volatile uint64_t *)first != first_value ||
        *(volatile uint64_t *)second != second_value ||
        !nova_mmu_translate((uintptr_t)first, &physical_address) ||
        physical_address < 0x40000000U || physical_address >= 0x48000000U) {
        (void)nova_vmap_free_page(first);
        (void)nova_vmap_free_page(second);
        return 0;
    }
    first_physical_address = physical_address;
    if (nova_page_pin_count((const void *)first_physical_address) != 1U ||
        nova_page_free((void *)first_physical_address)) {
        (void)nova_vmap_free_page(first);
        (void)nova_vmap_free_page(second);
        return 0;
    }

    if (!nova_vmap_free_page(first) ||
        nova_page_pin_count((const void *)first_physical_address) != 0U ||
        nova_page_is_allocated((const void *)first_physical_address) ||
        nova_vmap_free_page(first) ||
        nova_vmap_free_page((void *)((uintptr_t)second + 1U))) {
        (void)nova_vmap_free_page(second);
        return 0;
    }

    /* First-fit allocation should reuse the released virtual slot. */
    reused = nova_vmap_alloc_page(0U);
    if (reused != first ||
        !nova_mmu_verify_write_protected((uintptr_t)reused) ||
        !nova_mmu_verify_execute_never((uintptr_t)reused)) {
        if (reused != NULL) (void)nova_vmap_free_page(reused);
        (void)nova_vmap_free_page(second);
        return 0;
    }

    if (!nova_vmap_free_page(second) || !nova_vmap_free_page(reused))
        return 0;
    if (nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE |
                             NOVA_MMU_MAP_EXECUTABLE) != NULL) return 0;
    return 1;
}


static void test_task_entry(void *argument) {
    (void)argument;
}

static struct nova_cpu_context main_context;
static struct nova_cpu_context probe_context;
static volatile uint32_t context_switch_probe;

static void context_probe_entry(void) {
    context_switch_probe = 0x4E4F5641U;
    nova_arch_context_switch(&probe_context, &main_context);
    for (;;) __asm__ volatile("wfe");
}

static int test_arch_context_switch(void) {
    void *stack = nova_vmap_alloc_page(NOVA_MMU_MAP_WRITABLE);
    int passed;
    if (stack == NULL) return 0;
    context_switch_probe = 0;
    if (!nova_arch_context_init(&probe_context,
                                (uintptr_t)stack + NOVA_PAGE_SIZE,
                                (uintptr_t)context_probe_entry, 0, 0)) {
        (void)nova_vmap_free_page(stack);
        return 0;
    }
    nova_arch_context_switch(&main_context, &probe_context);
    passed = context_switch_probe == 0x4E4F5641U &&
             (main_context.sp & 15U) == 0U &&
             (probe_context.sp & 15U) == 0U;
    if (!nova_vmap_free_page(stack)) return 0;
    return passed;
}

static volatile uint32_t scheduler_order[4];
static volatile uint32_t scheduler_order_count;
static uint32_t scheduler_waiter_id;
static int scheduler_quantum_observed;

static void scheduler_task_a(void *argument) {
    (void)argument;
    uint64_t start_ticks;
    uint64_t deadline;
    uint64_t frequency;
    scheduler_order[scheduler_order_count++] = 1U;
    frequency = nova_timer_frequency();
    start_ticks = nova_irq_tick_count();
    if (frequency != 0U &&
        nova_timer_start_irq(frequency / 100U)) {
        __asm__ volatile("msr daifclr, #2\n\tisb" ::: "memory");
        deadline = nova_timer_now() + frequency;
        while (nova_irq_tick_count() - start_ticks < 5U &&
               nova_timer_now() < deadline)
            __asm__ volatile("nop");
        __asm__ volatile("msr daifset, #2\n\tisb" ::: "memory");
        nova_timer_stop();
        /*
         * A successful IRQ handoff consumes the pending request before the
         * interrupted task resumes. The task-order assertion below proves
         * the peer ran before this continuation.
         */
        scheduler_quantum_observed =
            (nova_irq_tick_count() - start_ticks >= 5U) &&
            !nova_sched_preemption_pending();
    }
    nova_sched_clear_preemption();
    (void)nova_sched_block_current();
    scheduler_order[scheduler_order_count++] = 3U;
}

static void scheduler_task_b(void *argument) {
    (void)argument;
    scheduler_order[scheduler_order_count++] = 2U;
    /* The waiter may already be READY because the timer IRQ preempted it. */
    if (!nova_sched_unblock(scheduler_waiter_id)) return;
    (void)nova_sched_yield();

    /* After the interrupted task resumes, it blocks cooperatively. */
    if (!nova_sched_unblock(scheduler_waiter_id)) return;
    (void)nova_sched_yield();
    scheduler_order[scheduler_order_count++] = 4U;
}

static int test_cooperative_scheduler(void) {
    uint32_t first, second;
    struct nova_task_info info;
    scheduler_order_count = 0U;
    scheduler_quantum_observed = 0;
    scheduler_order[0] = scheduler_order[1] = 0U;
    scheduler_order[2] = scheduler_order[3] = 0U;
    if (!nova_task_create(scheduler_task_a, 0, &first) ||
        !nova_task_create(scheduler_task_b, 0, &second)) return 0;
    scheduler_waiter_id = first;
    if (!nova_sched_start() || !scheduler_quantum_observed || scheduler_order_count != 4U ||
        scheduler_order[0] != 1U || scheduler_order[1] != 2U ||
        scheduler_order[2] != 3U || scheduler_order[3] != 4U ||
        nova_sched_current() != 0U ||
        nova_task_get(first, &info) ||
        nova_task_get(second, &info)) return 0;
    return 1;
}

static volatile uint64_t cpu_bound_start_tick;
static volatile uint64_t cpu_bound_progress_a;
static volatile uint64_t cpu_bound_progress_b;
static volatile uint32_t cpu_bound_b_started;
static volatile uint32_t cpu_bound_a_resumed;
static volatile uint32_t cpu_bound_b_resumed;

static void cpu_bound_task_a(void *argument) {
    uint64_t deadline = cpu_bound_start_tick + 20U;
    (void)argument;
    if (!nova_timer_start_irq(nova_timer_frequency() / 100U)) return;
    while (nova_irq_tick_count() < deadline) {
        ++cpu_bound_progress_a;
        if (cpu_bound_b_started) cpu_bound_a_resumed = 1U;
        __asm__ volatile("nop");
    }
}

static void cpu_bound_task_b(void *argument) {
    uint64_t deadline = cpu_bound_start_tick + 20U;
    (void)argument;
    cpu_bound_b_started = 1U;
    while (nova_irq_tick_count() < deadline) {
        ++cpu_bound_progress_b;
        if (cpu_bound_a_resumed) cpu_bound_b_resumed = 1U;
        __asm__ volatile("nop");
    }
    nova_timer_stop();
}

static void cleanup_scheduler_test_task(uint32_t task_id) {
    struct nova_task_info info;
    if (task_id == 0U || !nova_task_get(task_id, &info)) return;
    if (info.state == NOVA_TASK_READY &&
        info.resume_kind == NOVA_TASK_RESUME_FIRST_RUN) {
        (void)nova_task_destroy(task_id);
        return;
    }
    if (info.state == NOVA_TASK_BLOCKED)
        (void)nova_task_transition(task_id, NOVA_TASK_READY);
    if (nova_task_get(task_id, &info) && info.state == NOVA_TASK_READY)
        (void)nova_task_transition(task_id, NOVA_TASK_RUNNING);
    if (nova_task_get(task_id, &info) && info.state == NOVA_TASK_RUNNING)
        (void)nova_task_transition(task_id, NOVA_TASK_ZOMBIE);
    if (nova_task_get(task_id, &info) && info.state == NOVA_TASK_ZOMBIE)
        (void)nova_task_destroy(task_id);
}

static int test_irq_preemptive_scheduler(void) {
    uint32_t first = 0U, second = 0U;
    uint32_t space_a = 0U, space_b = 0U;
    uintptr_t root_a = 0U, root_b = 0U;
    struct nova_task_info info;
    struct nova_address_space_info address_space;
    int result = 0;

    cpu_bound_start_tick = nova_irq_tick_count();
    cpu_bound_progress_a = 0U;
    cpu_bound_progress_b = 0U;
    cpu_bound_b_started = 0U;
    cpu_bound_a_resumed = 0U;
    cpu_bound_b_resumed = 0U;

    /*
     * These tasks still execute privileged kernel code, but own distinct
     * TTBR0 roots. Their timer-driven handoff therefore exercises the real
     * cross-address-space path while all kernel stacks/code remain shared.
     */
    if (!nova_mmu_address_space_create(&root_a) ||
        !nova_mmu_address_space_create(&root_b) ||
        !nova_address_space_create(root_a, 11U, &space_a) ||
        !nova_address_space_create(root_b, 12U, &space_b) ||
        !nova_task_create(cpu_bound_task_a, 0, &first) ||
        !nova_task_create(cpu_bound_task_b, 0, &second) ||
        !nova_task_set_address_space(first, space_a) ||
        !nova_task_set_address_space(second, space_b))
        goto cleanup;

    if (!nova_sched_start()) {
        nova_timer_stop();
        goto cleanup;
    }

    /*
     * Neither entry calls Yield or Block. B starts only after A's first
     * timer preemption; A's post-B progress proves A resumed under its own
     * TTBR0; B's post-A progress proves the reverse handoff also completed.
     */
    result = cpu_bound_progress_a != 0U &&
             cpu_bound_progress_b != 0U &&
             cpu_bound_b_started &&
             cpu_bound_a_resumed &&
             cpu_bound_b_resumed &&
             nova_irq_tick_count() - cpu_bound_start_tick >= 20U &&
             nova_sched_current() == 0U &&
             !nova_task_get(first, &info) &&
             !nova_task_get(second, &info);

cleanup:
    nova_timer_stop();
    cleanup_scheduler_test_task(first);
    cleanup_scheduler_test_task(second);

    if (space_a != 0U &&
        nova_address_space_get(space_a, &address_space) &&
        address_space.references == 1U && address_space.active_cpus == 0U) {
        if (!nova_address_space_release(space_a) ||
            !nova_mmu_address_space_destroy(root_a))
            result = 0;
    } else if (space_a == 0U && root_a != 0U) {
        if (!nova_mmu_address_space_destroy(root_a)) result = 0;
    }
    if (space_b != 0U &&
        nova_address_space_get(space_b, &address_space) &&
        address_space.references == 1U && address_space.active_cpus == 0U) {
        if (!nova_address_space_release(space_b) ||
            !nova_mmu_address_space_destroy(root_b))
            result = 0;
    } else if (space_b == 0U && root_b != 0U) {
        if (!nova_mmu_address_space_destroy(root_b)) result = 0;
    }
    return result;
}

static int test_generic_timer(void) {
    uint64_t first, second, frequency, delay;
    uint32_t guard = 0U;
    if (!nova_timer_init()) return 0;
    frequency = nova_timer_frequency();
    if (frequency == 0U) return 0;
    first = nova_timer_now();
    while (guard++ < 10000U) __asm__ volatile("nop");
    second = nova_timer_now();
    if (second <= first) return 0;
    delay = frequency / 1000U;
    if (delay == 0U) delay = 1U;
    if (delay > 0x7fffffffULL) delay = 0x7fffffffULL;
    if (!nova_timer_arm(delay)) return 0;
    guard = 0U;
    while (!nova_timer_expired() && guard++ < 50000000U)
        __asm__ volatile("nop");
    if (!nova_timer_expired()) {
        nova_timer_stop();
        return 0;
    }
    nova_timer_stop();
    return 1;
}

static int test_timer_irq(void) {
    uint64_t frequency;
    uint64_t limit;
    if (!nova_timer_init() || !nova_gic_init()) return 0;
    frequency = nova_timer_frequency();
    if (frequency < 100U || !nova_timer_start_irq(frequency / 100U)) return 0;
    __asm__ volatile("msr daifclr, #2\n\tisb" ::: "memory");
    limit = nova_timer_now() + frequency;
    while (nova_irq_tick_count() < 3U && nova_timer_now() < limit)
        __asm__ volatile("nop");
    __asm__ volatile("msr daifset, #2\n\tisb" ::: "memory");
    nova_timer_stop();
    return nova_irq_tick_count() >= 3U;
}

static int test_task_lifecycle(void) {
    uint32_t first, second;
    struct nova_task_info info;
    struct nova_task_info second_info;
    struct nova_cpu_context *context;
    if (!nova_task_create(test_task_entry, (void *)0x1234U, &first) ||
        !nova_task_create(test_task_entry, 0, &second) || first == second)
        return 0;
    context = nova_task_context(first);
    if (context == 0 || !nova_task_get(first, &info) ||
        context->sp != (uintptr_t)info.irq_frame ||
        context->x19_x30[0] != (uintptr_t)test_task_entry ||
        context->x19_x30[1] != (uintptr_t)0x1234U ||
        context->x19_x30[2] == 0U || context->x19_x30[11] == 0U ||
        info.irq_frame == 0 ||
        info.irq_frame->x[0] != (uintptr_t)0x1234U ||
        info.irq_frame->x[19] != (uintptr_t)test_task_entry ||
        info.irq_frame->x[20] != (uintptr_t)0x1234U ||
        info.irq_frame->x[21] == 0U ||
        info.irq_frame->elr_el1 == 0U ||
        info.irq_frame->spsr_el1 != 0x5U ||
        info.state != NOVA_TASK_READY ||
        info.entry != test_task_entry || info.argument != (void *)0x1234U ||
        info.kernel_stack_top - info.kernel_stack_base != NOVA_PAGE_SIZE ||
        (info.kernel_stack_top & 15U) != 0U) return 0;
    if (info.kernel_stack_base < NOVA_PAGE_SIZE ||
        !nova_mmu_verify_unmapped(info.kernel_stack_base - NOVA_PAGE_SIZE) ||
        !nova_task_get(second, &second_info) ||
        second_info.kernel_stack_base < NOVA_PAGE_SIZE ||
        !nova_mmu_verify_unmapped(second_info.kernel_stack_base -
                                  NOVA_PAGE_SIZE))
        return 0;
    if (nova_task_transition(first, NOVA_TASK_BLOCKED) ||
        !nova_task_transition(first, NOVA_TASK_RUNNING) ||
        nova_task_destroy(first) ||
        !nova_task_set_exit_code(first, 7) ||
        !nova_task_transition(first, NOVA_TASK_BLOCKED) ||
        !nova_task_transition(first, NOVA_TASK_READY) ||
        !nova_task_transition(first, NOVA_TASK_RUNNING) ||
        !nova_task_transition(first, NOVA_TASK_ZOMBIE) ||
        !nova_task_get(first, &info) || info.exit_code != 7 ||
        !nova_task_destroy(first) || nova_task_get(first, &info) ||
        !nova_task_destroy(second)) return 0;
    return 1;
}

void kernel_main(uint64_t current_el, uintptr_t dtb_address) {
    int memory_regions;
    int reserved_regions;
    uint64_t total_ram = 0;
    size_t region_index;
    uart_write("Welcome to Nova OS\n");
    uart_write("AArch64 kernel prototype is running.\n");
    uart_write_current_el(current_el);

#ifdef NOVA_TEST_EL2_BOOT
    if (current_el == 1U) {
        uart_write("EL2-to-EL1 transition passed.\n");
    } else {
        uart_write("ERROR: EL2 boot did not enter EL1.\n");
    }
    for (;;) __asm__ volatile("wfe");
#endif

    memory_regions = nova_fdt_memory_regions(dtb_address, detected_memory,
                                             NOVA_FDT_MAX_MEMORY_REGIONS);
    if (memory_regions > 0) {
        for (region_index = 0; region_index < (size_t)memory_regions; ++region_index)
            total_ram += detected_memory[region_index].size;
        uart_write("FDT memory map parsed. Regions: ");
        uart_write_hex64((uint64_t)memory_regions);
        uart_write("FDT total RAM bytes: ");
        uart_write_hex64(total_ram);
    } else {
        uart_write("WARNING: FDT memory map unavailable.\n");
    }

    reserved_regions = nova_fdt_reserved_regions(
        dtb_address, reserved_memory, NOVA_FDT_MAX_RESERVED_REGIONS);
    if (reserved_regions >= 0) {
        uart_write("FDT reserved ranges parsed: ");
        uart_write_hex64((uint64_t)reserved_regions);
    } else {
        uart_write("WARNING: FDT reserved ranges unavailable.\n");
    }

    if (bss_probe == 0) uart_write("BSS zero initialization verified.\n");
    else uart_write("ERROR: BSS zero initialization failed.\n");

    if (test_heap()) uart_write("Heap allocator tests passed.\n");
    else uart_write("ERROR: heap allocator test failed.\n");

    if (test_pages()) uart_write("Physical page allocator tests passed.\n");
    else uart_write("ERROR: physical page allocator test failed.\n");

    if (memory_regions > 0 && reserved_regions >= 0 &&
        !test_firmware_pages(memory_regions, reserved_regions, dtb_address))
        uart_write("ERROR: firmware RAM page allocator test failed.\n");

    uart_write("Current page map total: ");
    uart_write_hex64(nova_page_total());
    uart_write("Current page map available: ");
    uart_write_hex64(nova_page_available());

    if (nova_mmu_enable()) {
        uart_write("EL1 MMU enabled with identity-mapped RAM and PL011.\n");
        if (nova_mmu_verify_identity((uintptr_t)&kernel_main) &&
            nova_mmu_verify_identity((uintptr_t)UART0_BASE) &&
            nova_mmu_verify_identity(0x08000000U) &&
            nova_mmu_verify_identity(0x08010000U)) {
            uart_write("MMU identity translation tests passed.\n");
        } else {
            uart_write("ERROR: MMU identity translation test failed.\n");
        }
        if (nova_mmu_verify_identity(0x40000000U) &&
            nova_mmu_verify_identity(0x40000fffU) &&
            nova_mmu_verify_identity(0x40001000U) &&
            nova_mmu_verify_identity(0x47ffffffU)) {
            uart_write("MMU 4 KiB RAM page boundary tests passed.\n");
        } else {
            uart_write("ERROR: MMU 4 KiB RAM page boundary test failed.\n");
        }
        if (nova_mmu_verify_unmapped(0x1000U) &&
            nova_mmu_verify_unmapped(0x48000000U)) {
            uart_write("MMU unmapped guard test passed.\n");
        } else {
            uart_write("ERROR: MMU unmapped guard address unexpectedly translated.\n");
        }
        if (nova_mmu_verify_executable((uintptr_t)&kernel_main) &&
            nova_mmu_verify_write_protected((uintptr_t)&kernel_main) &&
            nova_mmu_verify_write_protected((uintptr_t)__rodata_start) &&
            nova_mmu_verify_execute_never((uintptr_t)__rodata_start) &&
            nova_mmu_verify_writable((uintptr_t)&bss_probe) &&
            nova_mmu_verify_execute_never((uintptr_t)&bss_probe)) {
            uart_write("MMU kernel text/rodata/data permission tests passed.\n");
        } else {
            uart_write("ERROR: MMU kernel permission test failed.\n");
        }
        {
            uintptr_t root_a=0U,root_b=0U,frame_a,frame_b,translated=0U;
            uint32_t id_a=0U,id_b=0U; const uintptr_t user_va=0x80000000U; int ok=0;
            frame_a=(uintptr_t)nova_page_alloc(); frame_b=(uintptr_t)nova_page_alloc();
            if(frame_a&&frame_b&&nova_mmu_address_space_create(&root_a)&&
               nova_mmu_address_space_create(&root_b)&&
               nova_address_space_create(root_a,1U,&id_a)&&nova_address_space_create(root_b,2U,&id_b)&&
               nova_mmu_user_map_page(root_a,1U,user_va,frame_a,NOVA_MMU_MAP_WRITABLE)&&
               nova_mmu_user_map_page(root_b,2U,user_va,frame_b,NOVA_MMU_MAP_WRITABLE)&&
               nova_mmu_switch_address_space(root_a,1U)&&nova_mmu_translate(user_va,&translated)&&translated==frame_a&&
               nova_mmu_verify_user_access(user_va,0)&&nova_mmu_verify_user_access(user_va,1)&&
               nova_mmu_user_protect_page(root_a,1U,user_va,0U)&&
               nova_mmu_switch_address_space(root_a,1U)&&nova_mmu_verify_user_access(user_va,0)&&
               !nova_mmu_verify_user_access(user_va,1)&&
               nova_mmu_user_protect_page(root_a,1U,user_va,NOVA_MMU_MAP_EXECUTABLE)&&
               !nova_mmu_user_map_page(root_b,2U,user_va+NOVA_PAGE_SIZE,frame_a,NOVA_MMU_MAP_WRITABLE)&&
               nova_mmu_user_protect_page(root_a,1U,user_va,NOVA_MMU_MAP_WRITABLE)&&
               !nova_mmu_user_map_page(root_a,1U,user_va+NOVA_PAGE_SIZE,frame_b,NOVA_MMU_MAP_EXECUTABLE)&&
               nova_mmu_user_map_page(root_a,1U,user_va+NOVA_PAGE_SIZE,frame_b,0U)&&
               nova_mmu_switch_address_space(root_a,1U)&&
               nova_mmu_verify_user_access(user_va+NOVA_PAGE_SIZE,0)&&
               !nova_mmu_verify_user_access(user_va+NOVA_PAGE_SIZE,1)&&
               nova_mmu_user_unmap_page(root_a,1U,user_va+NOVA_PAGE_SIZE)&&
               nova_mmu_switch_address_space(root_b,2U)&&nova_mmu_translate(user_va,&translated)&&translated==frame_b&&
               nova_mmu_verify_user_access(user_va,1)&&
               nova_mmu_switch_address_space(nova_mmu_kernel_root(),0U)&&
               nova_address_space_release(id_a)&&nova_address_space_release(id_b)&&
               nova_mmu_user_unmap_page(root_a,1U,user_va)&&nova_mmu_user_unmap_page(root_b,2U,user_va)&&
               nova_mmu_address_space_destroy(root_a)&&nova_mmu_address_space_destroy(root_b)&&
               nova_page_free((void*)frame_a)&&nova_page_free((void*)frame_b))ok=1;
            if(ok)uart_write("Per-address-space user mapping, same-VA physical isolation, EL0 access, and teardown tests passed.\n");
            else{(void)nova_mmu_switch_address_space(nova_mmu_kernel_root(),0U);uart_write("ERROR: private user mapping/isolation test failed.\n");}
        }
        if (test_dynamic_virtual_pages()) {
            uart_write("MMU dynamic map/unmap and permission tests passed.\n");
        } else {
            uart_write("ERROR: MMU dynamic virtual page test failed.\n");
        }
        if (test_virtual_page_allocator()) {
            uart_write("Kernel virtual page allocator tests passed.\n");
        } else {
            uart_write("ERROR: kernel virtual page allocator test failed.\n");
        }
        if (test_task_lifecycle()) {
            uart_write("Kernel task lifecycle tests passed.\n");
        } else {
            uart_write("ERROR: kernel task lifecycle test failed.\n");
        }
        if (test_generic_timer()) {
            uart_write("EL1 generic physical timer tests passed.\n");
        } else {
            uart_write("ERROR: EL1 generic physical timer test failed.\n");
        }
        if (test_timer_irq()) {
            uart_write("GICv2 physical timer IRQ tests passed.\n");
        } else {
            uart_write("ERROR: GICv2 physical timer IRQ test failed.\n");
        }
        if (test_cooperative_scheduler()) {
            uart_write("Kernel mixed-mode scheduler tests passed.\n");
        } else {
            uart_write("ERROR: kernel cooperative scheduler test failed.\n");
        }
        if (test_irq_preemptive_scheduler()) {
            uart_write("Kernel cross-address-space CPU-bound IRQ preemption tests passed.\n");
        } else {
            uart_write("ERROR: kernel CPU-bound IRQ preemption test failed.\n");
        }
        if (test_arch_context_switch()) {
            uart_write("AArch64 cooperative context switch test passed.\n");
        } else {
            uart_write("ERROR: AArch64 cooperative context switch test failed.\n");
        }
    } else {
        uart_write("ERROR: EL1 MMU initialization rejected current state.\n");
    }

#ifdef NOVA_TEST_EL0_SVC
    if (current_el == 1U) {
        uart_write("Starting real EL0 SVC round-trip test.\n");
        if (test_el0_svc_round_trip())
            uart_write("QEMU EL0 SVC round-trip test passed.\n");
        else
            uart_write("ERROR: QEMU EL0 SVC round-trip test failed.\n");
    }
#endif

#ifdef NOVA_TEST_EL0_FAULT
    if (current_el == 1U) {
        uart_write("Starting EL0 data-abort containment test.\n");
        if (test_el0_fault_containment())
            uart_write("QEMU EL0 data-abort containment passed.\n");
        else
            uart_write("ERROR: QEMU EL0 data-abort containment failed.\n");
    }
#endif

#ifdef NOVA_TEST_EXCEPTION
    if (current_el == 1) {
        uart_write("Triggering controlled SVC exception test.\n");
        __asm__ volatile("svc #0" ::: "memory");
    }
#endif

#ifdef NOVA_TEST_MMU_FAULT
    if (current_el == 1) {
        volatile uint64_t fault_value;
        uart_write("Triggering controlled MMU translation fault test.\n");
        fault_value = *(volatile uint64_t *)0x1000U;
        (void)fault_value;
        uart_write("ERROR: unmapped MMU read unexpectedly succeeded.\n");
    }
#endif

#ifdef NOVA_TEST_STACK_GUARD_FAULT
    if (current_el == 1U) {
        uint32_t guard_task;
        struct nova_task_info guard_info;
        volatile uint64_t guard_value;
        uart_write("Triggering controlled task stack guard fault test.\n");
        if (!nova_task_create(test_task_entry, 0, &guard_task) ||
            !nova_task_get(guard_task, &guard_info)) {
            uart_write("ERROR: task stack guard setup failed.\n");
            for (;;) __asm__ volatile("wfe");
        }
        guard_value = *(volatile uint64_t *)(guard_info.kernel_stack_base -
                                              NOVA_PAGE_SIZE);
        (void)guard_value;
        uart_write("ERROR: task stack guard access unexpectedly succeeded.\n");
    }
#endif

    for (;;) __asm__ volatile("wfe");
}
