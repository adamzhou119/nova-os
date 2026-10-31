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

The DTB parser reports firmware-provided RAM regions and discovers reservations from both the FDT header memreserve map and `/reserved-memory` child `reg` properties. These ranges are currently diagnostic only: they are not yet subtracted from RAM or fed into the page allocator. The QEMU bare-metal test harness explicitly loads its generated DTB at 0x41000000 because QEMU's ELF loader does not populate the Linux boot-protocol x0 register. Real bootloaders should pass the DTB pointer in x0. The current heap and page pool are still fixed linker reservations for the QEMU prototype;
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


### Firmware-backed page map

The kernel now builds a page-address map from FDT RAM regions. It excludes the FDT header memreserve entries, /reserved-memory child ranges, the complete linker-defined kernel reservation (including the bootstrap heap and legacy test pool), and the live DTB blob itself. The QEMU smoke test allocates and frees pages from this map and checks that returned pages do not overlap any reservation.

This is an early single-core physical page allocator prototype: it uses a bounded static address index and bitmap, has no SMP locking, MMU mapping, reclaim, or hotplug support. The original fixed 4 MiB pool remains only for the bootstrap allocator regression test; the firmware-backed allocator is the active page map after that test.
