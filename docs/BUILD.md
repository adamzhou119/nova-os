# Build and test guide

## Target

The current prototype targets QEMU's AArch64 `virt` machine. It is an experimental bare-metal scaffold, not a phone image. See [the early boot contract](BOOT_PROTOCOL.md) for the current startup assumptions and limitations.

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
make -C kernel clean all test
```

Expected artifacts: `kernel/build/nova.elf` (normal boot) and `kernel/build/nova-test.elf` (controlled exception test).

Inspect it:

```sh
file kernel/build/nova.elf
readelf -h kernel/build/nova.elf
readelf -sW kernel/build/nova.elf | grep -E '__bss_start|__bss_end|_start'
```

The ELF machine should be AArch64. The entry point and load address must be consistent with the QEMU boot path. The linker exports the `.bss` bounds used by the assembly startup routine.

## QEMU smoke test

```sh
timeout 8s qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -nographic \
  -kernel kernel/build/nova.elf
```

The guest is expected to print a boot banner, its detected exception level (for example, `CurrentEL: EL1` or `CurrentEL: EL2`), and the BSS probe result:

```text
Welcome to Nova OS
AArch64 kernel prototype is running.
CurrentEL: EL1
BSS zero initialization verified.
```

The exception-level value is reported from the CPU register at runtime; the example above is illustrative, not a fixed expectation. The EL1 vector table reports exception syndrome and faulting/resume address, then halts rather than attempting an unsafe exception return.

QEMU is expected to remain running because the prototype parks in a wait loop. A timeout exit is therefore normal after output is observed. A timeout alone is not proof of successful boot; verify the serial text, including the BSS probe result.

## Troubleshooting

If there is no serial output, verify the ELF entry point, QEMU's kernel loader behavior, the selected exception level, UART base address, and whether the guest entered the expected execution state. Do not mark the runtime test successful without captured serial output.


## Controlled exception test

The `test` Make target builds a second image with `NOVA_TEST_EXCEPTION`:

```sh
timeout 8s qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -nographic \
  -kernel kernel/build/nova-test.elf
```

On the currently supported EL1 QEMU path, the test image executes `SVC #0`.
The expected diagnostic includes:

```text
Triggering controlled SVC exception test.
EXCEPTION: EL1 vector captured.
Vector: Current EL SPx synchronous
Vector slot: 0x...
ESR_EL1: 0x...
ELR_EL1: 0x...
System halted for diagnosis.
```

This is a controlled synchronous-exception test, not proof that IRQ, FIQ,
SError, lower-EL exceptions, or exception return are implemented.


## Secure EL3 → EL2 → EL1 transition test

CI also builds a dedicated transition image and runs it with QEMU's
TrustZone and virtualization extensions enabled:

```sh
timeout 8s qemu-system-aarch64 \\
  -machine virt,secure=on,virtualization=on,gic-version=2 \\
  -cpu cortex-a57 -nographic \\
  -device loader,file=kernel/build/el3-test-boot.elf,cpu-num=0 \\
  -device loader,file=kernel/build/nova-el2-test.elf
```

The `virt` machine's `secure` and `virtualization` options enable separate
Arm CPU extensions; both are required for this test. The guest is expected
to remain in its idle loop, so CI requires the expected timeout exit (124)
as well as the serial markers `Welcome to Nova OS`, `CurrentEL: EL1`, and
`EL2-to-EL1 transition passed.`. This validates the controlled QEMU path,
not EL2/EL3 vector handling or arbitrary firmware compatibility.
