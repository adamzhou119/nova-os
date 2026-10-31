# AArch64 Kernel Starter

This directory contains an experimental bare-metal kernel targeting QEMU's
AArch64 `virt` machine.

## Current scope

- AArch64 primary-core entry assembly and secondary-core parking.
- Explicit `.bss` zero initialization before entering C.
- Runtime reporting of the current exception level.
- EL1 exception vector table installation.
- Architectural EL1 vector-slot identification and terminal diagnostics for `ESR_EL1` and `ELR_EL1`.
- PL011 UART output.
- Normal and controlled-exception test ELF images.

This remains a bring-up scaffold, not a complete kernel or a phone operating
system. The exception handler is terminal: it does not restore context or
return from an exception. MMU, interrupt controller, scheduler, drivers,
networking, storage, and application runtime are not implemented.

## Requirements

- An AArch64 bare-metal-capable Clang toolchain.
- LLD.
- GNU Make.
- QEMU system emulator for AArch64 (`qemu-system-aarch64`).

Tool versions and a reproducible container/CI environment still need to be
pinned.

## Build

From this directory:

```sh
make clean all test
```

Expected outputs:

- `build/nova.elf`: normal boot image.
- `build/nova-test.elf`: image that deliberately executes `SVC #0` to test
  the EL1 synchronous exception path.

## Run the normal image

```sh
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -nographic \
  -kernel build/nova.elf
```

## Run the exception test image

```sh
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -nographic \
  -kernel build/nova-test.elf
```

The normal image prints the boot banner, detected exception level, and
`.bss` probe result. The test image additionally prints EL1 exception
syndrome and return-address diagnostics before halting.

Boot behavior is validated in GitHub Actions for the configured QEMU path.
This does not establish compatibility with a physical phone or arbitrary
boards.
