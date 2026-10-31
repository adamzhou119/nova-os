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
TEST_KERNEL_OBJECT := $(BUILD)/kernel-test.o

.PHONY: all test host-test clean
all: $(BUILD)/nova.elf

host-test: | $(BUILD)
\t$(CC) -std=c11 -Wall -Wextra -Werror -Iinclude tests/page_allocator_test.c src/page.c -o $(BUILD)/page-allocator-test
\t$(BUILD)/page-allocator-test

test: $(BUILD)/nova-test.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BOOT_OBJECT): src/boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(EXCEPTION_OBJECT): src/exceptions.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(MEMORY_OBJECT): src/memory.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(PAGE_OBJECT): src/page.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(FDT_OBJECT): src/fdt.c include/fdt.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(TEST_KERNEL_OBJECT): src/kernel.c include/memory.h include/fdt.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EXCEPTION -c $< -o $@

$(BUILD)/nova.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) -o $@

$(BUILD)/nova-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) $(PAGE_OBJECT) $(FDT_OBJECT) -o $@

clean:
	rm -rf $(BUILD)
