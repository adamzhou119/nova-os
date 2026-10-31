# AArch64 Kernel Starter

Experimental bare-metal kernel targeting QEMU's AArch64 `virt` machine.

## Implemented prototype features

- AArch64 primary-core entry and secondary-core parking.
- Explicit `.bss` zeroing and current exception-level reporting.
- EL1 exception vectors and terminal fault diagnostics.
- PL011 UART output.
- A 1 MiB bootstrap heap with checked aligned monotonic allocation.
- A bounded Flattened Device Tree (FDT) parser that discovers RAM regions and parses both the header memreserve map and `/reserved-memory` child `reg` entries.
- A bitmap-backed 4 KiB page allocator with allocation, free, double-free rejection, invalid-alignment rejection, and reuse checks.
- A firmware-backed physical page map built from FDT RAM regions, excluding the kernel image, DTB blob, and firmware-reserved ranges.
- QEMU checks for normal boot, heap/page allocator behavior, and a controlled EL1 `SVC #0` exception.

## Firmware-backed physical memory map

At boot, the kernel reads the FDT memory descriptors and reservation entries. The page allocator aligns each RAM region inward to complete 4 KiB pages, rejects malformed or overlapping RAM descriptors, and excludes every page intersecting a reserved range. The kernel reservation covers the image, BSS, bootstrap heap, and legacy test pool; the DTB blob is reserved separately.

The QEMU smoke test currently uses a 128 MiB `virt` RAM configuration and explicitly loads the generated DTB at `0x41000000`, because this ELF-loader test setup does not populate the Linux boot-protocol x0 register. The assembly entry has an explicit QEMU fallback address; a real bootloader should provide the DTB pointer in x0.

The firmware-backed map is the active page map after the legacy fixed-pool regression test. Its address index and bitmap are statically bounded to 32,768 pages (128 MiB at 4 KiB per page).

## Early EL1 MMU bring-up

The QEMU `virt` target now has an initial AArch64 stage-1 translation setup using a 4 KiB translation-table granule and a 39-bit TTBR0 virtual-address space. The prototype identity-maps the 1 GiB window beginning at `0x40000000` as Normal write-back memory and maps the 2 MiB PL011 UART window at `0x09000000` as Device-nGnRnE. It installs MAIR_EL1, TCR_EL1, and TTBR0_EL1, invalidates EL1 stage-1 TLB entries, then enables the EL1 MMU and data/instruction caches.

The QEMU smoke test verifies stage-1 identity translations for a kernel code address and the UART address after enabling translation, and confirms that an unmapped low virtual address faults as expected. This mapping is intentionally board-specific and minimal: it is not yet a general FDT-driven page-table builder, does not provide per-page permissions or user address spaces, and does not implement TTBR1 high-half mappings or dynamic map/unmap operations.

## Important limitations

This is still an early single-core kernel prototype, not a complete operating system or phone OS. The page allocator has no SMP locking, MMU integration, virtual-memory mappings, reclaim, or hotplug support. The exception handler is terminal and does not preserve or restore the complete register context. Interrupt-controller support, timer interrupts, scheduling, drivers, networking, storage, filesystem support, and the application/runtime layer are not implemented.

## Build and run

From the `kernel` directory:

```sh
make clean all test
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova-test.elf
```

The normal image exercises heap allocation and both page allocator paths. The test image also triggers an EL1 synchronous exception and prints diagnostic registers.
