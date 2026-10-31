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

The QEMU `virt` target now has an initial AArch64 stage-1 translation setup using a 4 KiB translation-table granule and a 39-bit TTBR0 virtual-address space. The prototype builds a three-level translation tree and identity-maps the complete 128 MiB QEMU RAM window beginning at `0x40000000` with 4 KiB Normal write-back leaf pages. It maps the 2 MiB PL011 UART window at `0x09000000` as Device-nGnRnE. It installs MAIR_EL1, TCR_EL1, and TTBR0_EL1, invalidates EL1 stage-1 TLB entries, then enables the EL1 MMU and data/instruction caches. RAM defaults to EL1 read/write with privileged and unprivileged execution disabled; linker-defined kernel text pages are EL1 read-only and executable, while rodata pages are read-only and execute-never. QEMU uses architectural address-translation instructions to verify write and instruction-fetch permissions for text, rodata, and BSS.

The QEMU smoke test verifies stage-1 identity translations for a kernel code address, the UART address, the first and last RAM pages, and adjacent 4 KiB page boundaries. It also confirms that unmapped low and end-of-RAM guard addresses fault as expected. This mapping is intentionally board-specific and minimal: it is not yet a general FDT-driven page-table builder, does not yet provide dynamic per-page map/unmap operations or user address spaces, and does not implement TTBR1 high-half mappings or dynamic map/unmap operations.

## Important limitations

This is still an early single-core kernel prototype, not a complete operating system or phone OS. The page allocator has no SMP locking, dynamic MMU map/unmap API, reclaim, or hotplug support. The current page tables are a static QEMU-specific identity map, not a general FDT-driven virtual-memory subsystem. The exception handler is terminal and does not preserve or restore the complete register context. Interrupt-controller support, timer interrupts, scheduling, drivers, networking, storage, filesystem support, and the application/runtime layer are not implemented.

## Build and run

From the `kernel` directory:

```sh
make clean all test
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova-test.elf
```

The normal image exercises heap allocation and both page allocator paths. The test image also triggers an EL1 synchronous exception and prints diagnostic registers.
