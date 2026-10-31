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
- QEMU checks for normal boot, heap/page allocator behavior, cooperative two-task scheduling, and a controlled EL1 `SVC #0` exception.
- An EL1 physical generic-timer driver reads the architectural counter/frequency and supports one-shot deadlines. The MMU maps the GICv2 distributor/CPU-interface window as Device memory. QEMU virt GICv2 support routes the physical timer PPI through an EL1 IRQ vector that saves/restores all general-purpose registers, acknowledges/EOIs the interrupt, rearms the timer, and returns with `eret`. This validates IRQ delivery and tick accounting; the scheduler is not yet switched from IRQ context, so timer-driven preemption is not implemented.

## Firmware-backed physical memory map

At boot, the kernel reads the FDT memory descriptors and reservation entries. The page allocator aligns each RAM region inward to complete 4 KiB pages, rejects malformed or overlapping RAM descriptors, and excludes every page intersecting a reserved range. The kernel reservation covers the image, BSS, bootstrap heap, and legacy test pool; the DTB blob is reserved separately.

The QEMU smoke test currently uses a 128 MiB `virt` RAM configuration and explicitly loads the generated DTB at `0x41000000`, because this ELF-loader test setup does not populate the Linux boot-protocol x0 register. The assembly entry has an explicit QEMU fallback address; a real bootloader should provide the DTB pointer in x0.

The firmware-backed map is the active page map after the legacy fixed-pool regression test. Its address index and bitmap are statically bounded to 32,768 pages (128 MiB at 4 KiB per page).

## Early EL1 MMU bring-up

The QEMU `virt` target now has an initial AArch64 stage-1 translation setup using a 4 KiB translation-table granule and a 39-bit TTBR0 virtual-address space. The prototype builds a three-level translation tree and identity-maps the complete 128 MiB QEMU RAM window beginning at `0x40000000` with 4 KiB Normal write-back leaf pages. It maps the 2 MiB PL011 UART window at `0x09000000` as Device-nGnRnE. It installs MAIR_EL1, TCR_EL1, and TTBR0_EL1, invalidates EL1 stage-1 TLB entries, then enables the EL1 MMU and data/instruction caches. RAM defaults to EL1 read/write with privileged and unprivileged execution disabled; linker-defined kernel text pages are EL1 read-only and executable, while rodata pages are read-only and execute-never. QEMU uses architectural address-translation instructions to verify write permissions for text, rodata, and BSS, and inspects the corresponding privileged execute-never bits in the page descriptors.

The QEMU smoke test verifies stage-1 identity translations for a kernel code address, the UART address, the first and last RAM pages, and adjacent 4 KiB page boundaries. It also confirms that unmapped low and end-of-RAM guard addresses fault as expected. The dynamic kernel virtual window supports 4 KiB map, unmap, and permission updates, with per-frame alias W^X tracking and physical-page pinning. This mapping remains board-specific: it is not yet a general FDT-driven page-table builder and does not implement TTBR1 high-half mappings or per-process address spaces.

## Important limitations

This is still an early single-core kernel prototype, not a complete operating system or phone OS. The page allocator has no SMP locking, reclaim, or hotplug support. The RAM page tables reserve a virtual-only window from `0x48000000` to `0x80000000` for dynamic 4 KiB mappings. `nova_mmu_map_page`, `nova_mmu_unmap_page`, and `nova_mmu_protect_page` validate page alignment and bounds, update leaf descriptors, and invalidate the affected EL1 TLB entry. Mapping flags support EL1 read-only/read-write and execute-never/executable choices with a per-mapping write-xor-execute (W^X) rule; user access remains disabled. The MMU now tracks writable and executable aliases per physical frame and rejects cross-alias W^X conflicts. Dynamic mapping also requires the physical frame to be currently allocated by the physical page allocator and pins it for each active mapping. The allocator refuses to free pinned frames; unmapping invalidates the TLB entry before dropping the pin. This is a mapping pin count, not yet a general-purpose process reference-count or concurrent SMP lifetime system. A small first-fit kernel virtual-page allocator now obtains physical pages, maps them into this window, and unmaps/frees them on release; it is single-core and currently allocates one page per call. QEMU exercises alias translation, write protection, permission changes, global W^X rejection across two aliases, allocated-frame ownership checks, alias read/write coherence, duplicate-map rejection, and unmap fault behavior. Host tests also cover physical-page ownership and pin/unpin lifetime checks; QEMU verifies that a mapped frame cannot be freed until its mapping is removed. This is a uniprocessor prototype without SMP page-table locking, general physical-device mapping, page-table reclamation, or per-process address spaces. The exception handler is terminal and does not preserve or restore the complete register context. A first task-control-block layer now tracks task IDs, entry/argument pairs, one-page kernel stacks, exit codes, validated READY/RUNNING/BLOCKED/ZOMBIE transitions, and an initialized first-run CPU context. The initial context carries the entry point and argument in callee-saved registers and returns through an assembly trampoline; if an entry function returns, the trampoline marks the task ZOMBIE and parks. Host tests verify the saved frame and trampoline address, while QEMU independently exercises an actual cooperative stack/context handoff and return. A cooperative AArch64 context frame now stores x19-x30 and SP according to the AAPCS64 callee-saved convention. An assembly switch routine saves the outgoing frame, restores the incoming frame, and returns on the incoming stack. QEMU performs a real stack/context handoff to a separately mapped page and back; host tests validate initial frame construction. Scheduling is cooperative and scans the bounded task table for READY tasks; there is no timer-interrupt preemption, interrupt-driven wakeup, priority policy, exception-frame restore, user-mode execution, or per-task address space. Context switching is single-core and does not yet include FPU/SIMD state, interrupt masking policy, or SMP synchronization. Interrupt-controller support, timer IRQ delivery/preemptive scheduling, general device drivers, networking, storage, filesystem support, and the application/runtime layer are not implemented.

## Build and run

From the `kernel` directory:

```sh
make clean all test
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova-test.elf
```

The normal image exercises heap allocation and both page allocator paths. The test image also triggers an EL1 synchronous exception and prints diagnostic registers.


## Scheduler quantum accounting

Timer IRQs also feed a five-tick (50 ms at 100 Hz) round-robin quantum counter. The scheduler raises a deferred preemption-pending flag when a running task exhausts its quantum; this stage only validates accounting and does not yet switch task contexts from IRQ context.
