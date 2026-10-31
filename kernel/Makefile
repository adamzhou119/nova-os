CLANG ?= clang
LD_LLD ?= ld.lld

TARGET := --target=aarch64-none-elf
CFLAGS := $(TARGET) -ffreestanding -fno-stack-protector -fno-pic \
          -fno-builtin -nostdlib -O2 -Wall -Wextra
ASFLAGS := $(TARGET) -ffreestanding -fno-pic
LDFLAGS := -m aarch64elf -T linker.ld -nostdlib

BUILD := build
OBJECTS := $(BUILD)/boot.o $(BUILD)/kernel.o

.PHONY: all clean
all: $(BUILD)/nova.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: src/boot.S | $(BUILD)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(BUILD)/kernel.o: src/kernel.c | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/nova.elf: $(OBJECTS) linker.ld
	$(LD_LLD) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -rf $(BUILD)
