# AArch64 Kernel Starter

Experimental bare-metal starter targeting QEMU's AArch64 `virt` machine.

## Scope
- AArch64 entry assembly.
- Minimal C kernel entry.
- PL011 UART output.
- Linker script and Makefile.

This is a bring-up scaffold, not a complete kernel. It has not yet been runtime-verified in QEMU in this repository.

## Requirements
- Clang with AArch64 bare-metal target support.
- LLD.
- QEMU system emulator for AArch64 (`qemu-system-aarch64`).

Tool versions and a reproducible container/CI environment still need to be pinned.

## Build
From this directory: `make`. Expected output: `build/nova.elf`.

## Experimental run command
```sh
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
```

Boot behavior must be checked against the installed QEMU version. If the guest does not reach UART output, inspect ELF entry/load address, exception level, and boot protocol.

## Intended serial smoke-test message
`Welcome to Nova OS`

This is the intended message, not evidence of a completed runtime test.
