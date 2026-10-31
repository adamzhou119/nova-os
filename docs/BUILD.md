# Build and test guide

## Target

The current prototype targets QEMU's AArch64 `virt` machine. It is an experimental bare-metal scaffold, not a phone image.

## Toolchain

The build expects:
- Clang with the `aarch64-none-elf` target enabled.
- LLD (`ld.lld`).
- GNU Make.
- QEMU system emulator for AArch64 for runtime smoke testing.

On Ubuntu, a starting package set is:

```sh
sudo apt-get update
sudo apt-get install -y clang lld make qemu-system-arm
```

Package versions vary by distribution. Pin exact versions in CI or a development container before treating builds as reproducible.

## Compile

```sh
make -C kernel clean all
```

Expected artifact: `kernel/build/nova.elf`.

Inspect it:

```sh
file kernel/build/nova.elf
llvm-readelf -h kernel/build/nova.elf
```

The ELF machine should be AArch64. The entry point and load address must be consistent with the QEMU boot path.

## QEMU smoke test

```sh
timeout 8s qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -nographic \
  -kernel kernel/build/nova.elf
```

The guest is expected to print:

```text
Welcome to Nova OS
AArch64 kernel prototype is running.
```

QEMU is expected to remain running because the prototype parks in a wait loop. A timeout exit is therefore normal after output is observed. A timeout alone is not proof of successful boot; verify the serial text.

## Troubleshooting

If there is no serial output, verify the ELF entry point, QEMU's kernel loader behavior, the selected exception level, UART base address, and whether the guest entered the expected execution state. Do not mark the runtime test successful without captured serial output.
