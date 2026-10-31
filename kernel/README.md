# AArch64 Kernel Starter

Experimental bare-metal kernel targeting QEMU's AArch64 `virt` machine.

## Implemented prototype features

- AArch64 primary-core entry and secondary-core parking.
- Explicit `.bss` zeroing and current exception-level reporting.
- EL1 exception vectors and terminal fault diagnostics.
- PL011 UART output.
- A 1 MiB fixed bootstrap heap with checked aligned monotonic allocation.
- A bounded Flattened Device Tree (FDT) parser that discovers RAM regions and parses the header memreserve map plus `/reserved-memory` child ranges.\n- A 4 MiB fixed page pool with a bitmap-backed 4 KiB page allocator,
  allocation, free, double-free rejection, invalid-alignment rejection, and
  page reuse checks.
- QEMU checks for normal boot, heap/page allocator behavior, and a controlled
  EL1 `SVC #0` exception.

## Important limitations

The DTB parser reports firmware-provided RAM regions. The QEMU bare-metal test harness explicitly loads its generated DTB at 0x41000000 because QEMU's ELF loader does not populate the Linux boot-protocol x0 register. Real bootloaders should pass the DTB pointer in x0. The current heap and page pool are still fixed linker reservations for the QEMU prototype;
they are not discovered from firmware or a board memory map. The page allocator
is single-core only, has no locking, and is not yet connected to an MMU or
virtual-memory subsystem. There is no paging, page-table management, reclaim,
or memory protection. The exception handler is terminal and does not preserve
or restore the complete register context. Interrupt controller, scheduler,
drivers, networking, storage, and app runtime are not implemented. This is
not yet a complete kernel or phone OS.

## Build and run

```sh
make clean all test
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova-test.elf
```

The normal image tests heap and physical page allocation. The test image also
triggers an EL1 synchronous exception and prints diagnostic registers.
