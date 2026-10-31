# AArch64 Kernel Starter

Experimental bare-metal kernel targeting QEMU's AArch64 `virt` machine.

## Implemented prototype features

- AArch64 primary-core entry and secondary-core parking.
- Explicit `.bss` zeroing and current exception-level reporting.
- EL1 exception vectors and terminal fault diagnostics (`ESR_EL1`, `ELR_EL1`,
  `FAR_EL1`, `SPSR_EL1`).
- PL011 UART output.
- A 1 MiB linker-reserved bootstrap heap and a checked, aligned monotonic
  allocator (`nova_alloc`).
- QEMU checks for normal boot, heap alignment/allocation/exhaustion, and a
  controlled EL1 `SVC #0` exception.

## Important limitations

The heap is a fixed region reserved by this QEMU prototype's linker script;
it is not discovered from firmware or a board memory map. The allocator is
monotonic: there is no free operation, page allocator, MMU, paging, or SMP
locking. Do not use it concurrently from secondary CPUs. The exception handler
is terminal and does not preserve/restore the complete register context.
Interrupt controller, scheduler, drivers, networking, storage, and app runtime
are not implemented. This is not yet a complete kernel or phone OS.

## Build and run

```sh
make clean all test
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova-test.elf
```

The normal image reports heap test results. The test image additionally
triggers an EL1 synchronous exception and prints diagnostic registers.
