# AArch64 Kernel Starter

This directory contains an experimental bare-metal starter targeting QEMU's AArch64 `virt` machine.

## Current scope

- AArch64 entry assembly.
- A small C kernel entry point.
- PL011 UART text output.
- A linker script and a minimal Makefile.

This is a bring-up scaffold only. It is not a complete kernel and has not yet been runtime-verified in QEMU in this repository.

## Requirements

- An AArch64 bare-metal-capable Clang toolchain.
- LLD.
- QEMU system emulator for AArch64 (`qemu-system-aarch64`).

Tool versions and a reproducible container/CI environment still need to be pinned.

## Build

From this directory:

```sh
make
```

The expected output is `build/nova.elf`.

## Run

A typical QEMU invocation for the current experimental setup is:

```sh
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -nographic \
  -kernel build/nova.elf
```

Boot behavior must be verified against the installed QEMU version. If QEMU reports an unsupported image or the guest does not reach UART output, inspect the ELF entry point, load address, exception level, and boot protocol rather than assuming success.

## Expected serial message

`Welcome to Nova OS`

This is the intended smoke-test message, not a claim that the runtime test has already passed.
