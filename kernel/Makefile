CLANG ?= clang
LD_LLD ?= ld.lld

TARGET := --target=aarch64-none-elf
CFLAGS := $(TARGET) -ffreestanding -fno-stack-protector -fno-pic \
          -fno-builtin -nostdlib -O2 -Wall -Wextra -Werror -Ikernel/include
ASFLAGS := $(TARGET) -ffreestanding -fno-pic
LDFLAGS := -m aarch64elf -T linker.ld -nostdlib

BUILD := build
BOOT_OBJECT := $(BUILD)/boot.o
EXCEPTION_OBJECT := $(BUILD)/exceptions.o
KERNEL_OBJECT := $(BUILD)/kernel.o
MEMORY_OBJECT := $(BUILD)/memory.o
TEST_KERNEL_OBJECT := $(BUILD)/kernel-test.o

.PHONY: all test clean
all: $(BUILD)/nova.elf

test: $(BUILD)/nova-test.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BOOT_OBJECT): src/boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(EXCEPTION_OBJECT): src/exceptions.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(KERNEL_OBJECT): src/kernel.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(MEMORY_OBJECT): src/memory.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(TEST_KERNEL_OBJECT): src/kernel.c include/memory.h | $(BUILD)
	$(CLANG) $(CFLAGS) -DNOVA_TEST_EXCEPTION -c $< -o $@

$(BUILD)/nova.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(KERNEL_OBJECT) $(MEMORY_OBJECT) -o $@

$(BUILD)/nova-test.elf: $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) linker.ld
	$(LD_LLD) $(LDFLAGS) $(BOOT_OBJECT) $(EXCEPTION_OBJECT) $(TEST_KERNEL_OBJECT) $(MEMORY_OBJECT) -o $@

clean:
	rm -rf $(BUILD)
