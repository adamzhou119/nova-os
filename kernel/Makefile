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
MEMORY_OBJECT := $(BUILD)/memory.o
PAGE_OBJECT := $(BUILD)/page.o
FDT_OBJECT := $(BUILD)/fdt.o
MMU_OBJECT := $(BUILD)/mmu.o
VM_OBJECT := $(BUILD)/vm.o
TASK_OBJECT := $(BUILD)/task.o
CONTEXT_OBJECT := $(BUILD)/context.o
CONTEXT_ASM_OBJECT := $(BUILD)/context-asm.o
TEST_KERNEL_OBJECT := $(BUILD)/kernel-test.o
FAULT_TEST_KERNEL_OBJECT := $(BUILD)/kernel-fault-test.o

.PHONY: all test host-test clean
all: $(BUILD)/nova.elf

host-test: | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/page_allocator_test.c src/page.c -o $(BUILD)/page-allocator-test
	$(BUILD)/page-allocator-test
	$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/task_test.c src/task.c src/context.c -o $(BUILD)/task-test
	$(BUILD)/task-test

test: $(BUILD)/nova-test.elf $(BUILD)/nova-fault-test.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BOOT_OBJECT): src/boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(EXCEPTION_OBJECT): src/exceptions.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(CONTEXT_ASM_OBJECT): src/context.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(CONTEXT_OBJECT): src/context.c include/context.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h include/context.h | $(BUILD)
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

$(TASK_OBJECT): src/task.c include/task.h include/vm.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EXCEPTION -c $< -o $@

$(FAULT_TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h include/mmu.h include/vm.h include/task.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_MMU_FAULT -c $< -o $@

$(BUILD)/nova.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) $(CONTEXT_OBJECT) $(CONTEXT_ASM_OBJECT) -o $@

$(BUILD)/nova-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) -o $@

$(BUILD)/nova-fault-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(FAULT_TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) $(MMU_OBJECT) $(VM_OBJECT) $(TASK_OBJECT) -o $@

clean:
	rm -rf $(BUILD)
