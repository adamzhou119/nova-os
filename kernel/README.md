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
- QEMU checks for normal boot, heap/page allocator behavior, mixed cooperative/IRQ scheduling, two CPU-bound tasks that make progress through repeated timer preemption without Yield/Block, task-stack guard fault verification, deferred task reclamation, a real AArch64 stack/context handoff, and controlled EL1 `SVC #0` and MMU translation-fault exceptions.
- An EL1 physical generic-timer driver reads the architectural counter/frequency and supports one-shot deadlines. The MMU maps the GICv2 distributor/CPU-interface window as Device memory. QEMU virt GICv2 routes the physical timer PPI through an EL1 IRQ vector that saves x0-x30, ELR_EL1, and SPSR_EL1, acknowledges/EOIs the interrupt, rearms the timer, and returns with `eret`. After five ticks, the dispatcher can hand off to another IRQ-returnable task frame. The scheduler restores saved IRQ contexts through a dedicated ERET path and cooperative contexts through the AAPCS64 switch path.

## Firmware-backed physical memory map

At boot, the kernel reads the FDT memory descriptors and reservation entries. The page allocator aligns each RAM region inward to complete 4 KiB pages, rejects malformed or overlapping RAM descriptors, and excludes every page intersecting a reserved range. The kernel reservation covers the image, BSS, bootstrap heap, and legacy test pool; the DTB blob is reserved separately.

The QEMU smoke test currently uses a 128 MiB `virt` RAM configuration and explicitly loads the generated DTB at `0x41000000`, because this ELF-loader test setup does not populate the Linux boot-protocol x0 register. The assembly entry has an explicit QEMU fallback address; a real bootloader should provide the DTB pointer in x0.

The firmware-backed map is the active page map after the legacy fixed-pool regression test. Its address index and bitmap are statically bounded to 32,768 pages (128 MiB at 4 KiB per page).

## Early EL1 MMU bring-up

The QEMU `virt` target now has an initial AArch64 stage-1 translation setup using a 4 KiB translation-table granule and a 39-bit TTBR0 virtual-address space. The prototype builds a three-level translation tree and identity-maps the complete 128 MiB QEMU RAM window beginning at `0x40000000` with 4 KiB Normal write-back leaf pages. It maps the 2 MiB PL011 UART window at `0x09000000` as Device-nGnRnE. It installs MAIR_EL1, TCR_EL1, and TTBR0_EL1, invalidates EL1 stage-1 TLB entries, then enables the EL1 MMU and data/instruction caches. RAM defaults to EL1 read/write with privileged and unprivileged execution disabled; linker-defined kernel text pages are EL1 read-only and executable, while rodata pages are read-only and execute-never. QEMU uses architectural address-translation instructions to verify write permissions for text, rodata, and BSS, and inspects the corresponding privileged execute-never bits in the page descriptors.

The QEMU smoke test verifies stage-1 identity translations for a kernel code address, the UART address, the first and last RAM pages, and adjacent 4 KiB page boundaries. It also confirms that unmapped low and end-of-RAM guard addresses fault as expected. The dynamic kernel virtual window supports 4 KiB map, unmap, and permission updates, with per-frame alias W^X tracking and physical-page pinning. This mapping remains board-specific: it is not yet a general FDT-driven page-table builder and does not implement TTBR1 high-half mappings or per-process address spaces.

## Important limitations

This is still an early single-core kernel prototype, not a complete operating system or phone OS. The page allocator has no SMP locking, reclaim, or hotplug support. The RAM page tables reserve a virtual-only window from `0x48000000` to `0x80000000` for dynamic 4 KiB mappings. `nova_mmu_map_page`, `nova_mmu_unmap_page`, and `nova_mmu_protect_page` validate page alignment and bounds, update leaf descriptors, and invalidate the affected EL1 TLB entry. Mapping flags support EL1 read-only/read-write and execute-never/executable choices with a per-mapping write-xor-execute (W^X) rule; user access remains disabled. The MMU now tracks writable and executable aliases per physical frame and rejects cross-alias W^X conflicts. Dynamic mapping also requires the physical frame to be currently allocated by the physical page allocator and pins it for each active mapping. The allocator refuses to free pinned frames; unmapping invalidates the TLB entry before dropping the pin. This is a mapping pin count, not yet a general-purpose process reference-count or concurrent SMP lifetime system. A small first-fit kernel virtual-page allocator now obtains physical pages, maps them into this window, and unmaps/frees them on release; it is single-core and currently allocates one page per call. QEMU exercises alias translation, write protection, permission changes, global W^X rejection across two aliases, allocated-frame ownership checks, alias read/write coherence, duplicate-map rejection, and unmap fault behavior. Host tests also cover physical-page ownership and pin/unpin lifetime checks; QEMU verifies that a mapped frame cannot be freed until its mapping is removed. This is a uniprocessor prototype without SMP page-table locking, general physical-device mapping, page-table reclamation, or per-process address spaces. Synchronous exception handlers remain terminal diagnostics. The IRQ entry path separately preserves and restores the complete integer register frame plus ELR_EL1/SPSR_EL1. A first task-control-block layer now tracks task IDs, entry/argument pairs, one-page kernel stacks, exit codes, validated READY/RUNNING/BLOCKED/ZOMBIE transitions, and an initialized first-run CPU context. The initial context carries the entry point and argument in callee-saved registers and returns through an assembly trampoline; if an entry function returns, the trampoline marks the task ZOMBIE and parks. Host tests verify the saved frame and trampoline address, while QEMU independently exercises an actual cooperative stack/context handoff and return. A cooperative AArch64 context frame now stores x19-x30 and SP according to the AAPCS64 callee-saved convention. An assembly switch routine saves the outgoing frame, restores the incoming frame, and returns on the incoming stack. QEMU performs a real stack/context handoff to a separately mapped page and back; host tests validate initial frame construction. Scheduling is single-core and scans the bounded task table for READY tasks in round-robin order. Tasks can yield/block cooperatively, and timer IRQs can preempt a task after a five-tick quantum by saving its exception frame and returning another task's frame. The scheduler distinguishes FIRST_RUN, COOPERATIVE, IRQ, and ACTIVE resume kinds; it consumes saved IRQ frames once and reaps ZOMBIE tasks only after returning to the scheduler stack. There is no priority policy, user-mode execution, or per-task address space. Context switching is single-core and does not yet include FPU/SIMD state or SMP synchronization. The GICv2 and generic timer drivers are validated for the QEMU virt target. This remains a uniprocessor prototype; there is no SMP scheduler, FIQ/SError scheduling, user-mode execution, or FP/SIMD context preservation. General device drivers beyond UART/timer/GIC, networking, storage, filesystem support, and the application/runtime layer are not implemented.

## Build and run

From the `kernel` directory:

```sh
make clean all test
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova.elf
qemu-system-aarch64 -machine virt -cpu cortex-a57 -nographic -kernel build/nova-test.elf
```

The normal image exercises heap allocation and both page allocator paths. The test image also triggers an EL1 synchronous exception and prints diagnostic registers.


## Scheduler quantum accounting

Timer IRQs feed a five-tick (50 ms at 100 Hz) round-robin quantum counter while a task is running. When the quantum expires, the IRQ dispatcher saves the interrupted task's frame and returns a selected task frame. QEMU validates both a mixed cooperative/IRQ handoff and repeated preemption between two CPU-bound tasks that never call Yield or Block.

### IRQ exception-frame integrity

- The EL1 SPx IRQ vector no longer overwrites x16 to carry a diagnostic vector number; the interrupted x16 value is preserved.
- IRQ entry now saves and restores x0-x30, ELR_EL1, and SPSR_EL1 in a 272-byte, 16-byte-aligned frame.
- This provides the complete integer-register frame required for safe IRQ-driven task switching. The scheduler uses the frame pointer returned by the dispatcher to restore the selected task.

### IRQ frame restore ABI

- The C IRQ dispatcher receives the current 272-byte exception-frame address and returns the frame address that the vector entry must restore.
- The assembly entry switches its restore SP to that returned address before restoring registers and executing `eret`.
- The timer dispatcher returns the same frame when no alternative task is eligible, or a different validated task frame when the quantum expires and another IRQ-returnable task is ready.
- The C structure in `interrupt.h` statically checks the assembly offsets and total frame size. Host-side IRQ dispatcher tests validate timer PPI accounting, scheduler tick notification, timer rearming at 100 Hz, EOI behavior for normal interrupts, spurious interrupt handling, and frame-pointer preservation. Host tests validate the dispatcher ABI; the QEMU mixed-mode and CPU-bound scheduler tests validate actual exception-frame returns and repeated timer preemption.
