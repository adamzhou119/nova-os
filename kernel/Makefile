CLANG ?= clang
LD_LLD ?= ld.lld
CC ?= cc

TARGET := --target=aarch64-none-elf
CFLAGS := $(TARGET) -ffreestanding -fno-stack-protector -fno-pic \
          -fno-builtin -mgeneral-regs-only -nostdlib -O2 -Wall -Wextra -Werror -Iinclude
ASFLAGS := $(TARGET) -ffreestanding -fno-pic
LDFLAGS := -m aarch64elf -T linker.ld -nostdlib

BUILD := build
BOOT_OBJECT := $(BUILD)/boot.o
EXCEPTION_OBJECT := $(BUILD)/exceptions.o
KERNEL_OBJECT := $(BUILD)/kernel.o
CONSOLE_OBJECT := $(BUILD)/console.o
DEVICE_OBJECT := $(BUILD)/device.o
MEMORY_OBJECT := $(BUILD)/memory.o
PAGE_OBJECT := $(BUILD)/page.o
FDT_OBJECT := $(BUILD)/fdt.o
MMU_OBJECT := $(BUILD)/mmu.o
VM_OBJECT := $(BUILD)/vm.o
TASK_OBJECT := $(BUILD)/task.o
CONTEXT_OBJECT := $(BUILD)/context.o
CONTEXT_ASM_OBJECT := $(BUILD)/context-asm.o
SCHEDULER_OBJECT := $(BUILD)/scheduler.o
TIMER_OBJECT := $(BUILD)/timer.o
GIC_OBJECT := $(BUILD)/gic.o
INTERRUPT_OBJECT := $(BUILD)/interrupt.o
EXCEPTION_CLASS_OBJECT := $(BUILD)/exception.o
ADDRESS_SPACE_OBJECT := $(BUILD)/address_space.o
USER_MEMORY_OBJECT := $(BUILD)/user_memory.o
TEST_KERNEL_OBJECT := $(BUILD)/kernel-test.o
FAULT_TEST_KERNEL_OBJECT := $(BUILD)/kernel-fault-test.o
STACK_FAULT_TEST_KERNEL_OBJECT := $(BUILD)/kernel-stack-fault-test.o
EL2_TEST_KERNEL_OBJECT := $(BUILD)/kernel-el2-test.o
EL2_VECTOR_BOOT_OBJECT := $(BUILD)/boot-el2-vector.o
USER_TEST_KERNEL_OBJECT := $(BUILD)/kernel-user-test.o
USER_FAULT_TEST_KERNEL_OBJECT := $(BUILD)/kernel-user-fault-test.o
EL3_TEST_BOOT_OBJECT := $(BUILD)/el3-test-boot.o

.PHONY: all test host-test benchmark clean
all: $(BUILD)/nova.elf

host-test: | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/page_allocator_test.c src/page.c -o $(BUILD)/page-allocator-test
	$(BUILD)/page-allocator-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/vm_test.c src/vm.c -o $(BUILD)/vm-test
	$(BUILD)/vm-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/device_test.c src/device.c src/fdt.c -o $(BUILD)/device-test
	$(BUILD)/device-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/fdt_device_test.c src/fdt.c src/device.c -o $(BUILD)/fdt-device-test
	$(BUILD)/fdt-device-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/task_test.c src/task.c src/context.c src/address_space.c -o $(BUILD)/task-test
	$(BUILD)/task-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/interrupt_test.c src/interrupt.c -o $(BUILD)/interrupt-test
	$(BUILD)/interrupt-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/exception_test.c src/exception.c -o $(BUILD)/exception-test
	$(BUILD)/exception-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/address_space_test.c src/address_space.c -o $(BUILD)/address-space-test
	$(BUILD)/address-space-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/scheduler_test.c src/scheduler.c src/task.c src/context.c src/address_space.c -o $(BUILD)/scheduler-test
	$(BUILD)/scheduler-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/user_context_test.c src/context.c -o $(BUILD)/user-context-test
	$(BUILD)/user-context-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/user_memory_test.c src/user_memory.c -o $(BUILD)/user-memory-test
	$(BUILD)/user-memory-test

benchmark: | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/page_allocator_bench.c src/page.c -o $(BUILD)/page-allocator-bench
	$(BUILD)/page-allocator-bench

test: $(BUILD)/nova-test.elf $(BUILD)/nova-fault-test.elf $(BUILD)/nova-stack-fault-test.elf $(BUILD)/nova-el2-test.elf $(BUILD)/nova-el2-vector-test.elf $(BUILD)/nova-user-test.elf $(BUILD)/nova-user-fault-test.elf $(BUILD)/el3-test-boot.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BOOT_OBJECT): src/boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(EL2_VECTOR_BOOT_OBJECT): src/boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -DNOVA_TEST_EL2_VECTOR -c $< -o $@

$(EXCEPTION_OBJECT): src/exceptions.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(CONTEXT_ASM_OBJECT): src/context.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(CONTEXT_OBJECT): src/context.c include/context.h include/user_context.h include/interrupt.h include/mmu.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(CONSOLE_OBJECT): src/console.c include/console.h include/device.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(DEVICE_OBJECT): src/device.c include/device.h include/fdt.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/user_context.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(MEMORY_OBJECT): src/memory.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(PAGE_OBJECT): src/page.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(FDT_OBJECT): src/fdt.c include/fdt.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(MMU_OBJECT): src/mmu.c include/mmu.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(VM_OBJECT): src/vm.c include/vm.h include/mmu.h include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(TASK_OBJECT): src/task.c include/task.h include/context.h include/interrupt.h include/user_context.h include/vm.h include/address_space.h include/mmu.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(SCHEDULER_OBJECT): src/scheduler.c include/scheduler.h include/task.h include/context.h include/interrupt.h include/exception.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(TIMER_OBJECT): src/timer.c include/timer.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(GIC_OBJECT): src/gic.c include/gic.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(INTERRUPT_OBJECT): src/interrupt.c include/interrupt.h include/gic.h include/timer.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(EXCEPTION_CLASS_OBJECT): src/exception.c include/exception.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(ADDRESS_SPACE_OBJECT): src/address_space.c include/address_space.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(USER_MEMORY_OBJECT): src/user_memory.c include/user_memory.h include/mmu.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EXCEPTION -c $< -o $@

$(FAULT_TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_MMU_FAULT -c $< -o $@

$(STACK_FAULT_TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_STACK_GUARD_FAULT -c $< -o $@

$(USER_TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/address_space.h include/user_context.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EL0_SVC -c $< -o $@

$(USER_FAULT_TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/address_space.h include/user_context.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EL0_FAULT -c $< -o $@

$(EL2_TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h include/scheduler.h include/timer.h include/gic.h include/interrupt.h include/exception.h include/user_memory.h include/console.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EL2_BOOT -c $< -o $@

$(EL3_TEST_BOOT_OBJECT): src/el3_test_boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(BUILD)/el3-test-boot.elf: $(EL3_TEST_BOOT_OBJECT) el3_test_boot.ld
	$(LD_LLD) -m aarch64elf -T el3_test_boot.ld -nostdlib $< -o $@

$(BUILD)/nova.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@

$(BUILD)/nova-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@

$(BUILD)/nova-fault-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@

$(BUILD)/nova-stack-fault-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(STACK_FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(STACK_FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@

$(BUILD)/nova-el2-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(EL2_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker-el2-test.ld
	$(LD_LLD) -m aarch64elf -T linker-el2-test.ld -nostdlib $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(EL2_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@
$(BUILD)/nova-user-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(USER_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(USER_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@

$(BUILD)/nova-user-fault-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(USER_FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(USER_FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@


$(BUILD)/nova-el2-vector-test.elf: $(EL2_VECTOR_BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(EL2_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) linker-el2-test.ld
	$(LD_LLD) -m aarch64elf -T linker-el2-test.ld -nostdlib $(EL2_VECTOR_BOOT_OBJECT) $(EXCEPTION_OBJECT) $(CONSOLE_OBJECT) $(DEVICE_OBJECT) $(EL2_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) $(SCHEDULER_OBJECT) $(TIMER_OBJECT) $(GIC_OBJECT) $(INTERRUPT_OBJECT) $(EXCEPTION_CLASS_OBJECT) $(ADDRESS_SPACE_OBJECT) $(USER_MEMORY_OBJECT) -o $@


clean:
	rm -rf $(BUILD)
