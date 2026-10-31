# Early Boot Contract

This document records the current experimental boot path for the AArch64 QEMU
`virt` prototype. It is narrower than a general firmware, server, or phone
boot specification.

## Current sequence

1. The image enters at the linker entry symbol `_start`.
2. The entry code preserves the DTB pointer in x20. The QEMU ELF-loader test
   path uses the documented fallback address when x0 is unset.
3. The primary CPU establishes the bootstrap stack. Secondary CPUs park before
   touching shared initialization; per-CPU stacks and SMP startup are not
   implemented.
4. The primary CPU clears the linker-defined `__bss_start..__bss_end` range,
   then completes the initialization with DSB and ISB.
5. The entry code reads `CurrentEL[3:2]` and passes the decoded exception level
   to `kernel_main`, which reports it over PL011 UART.
6. When running at EL1, the entry code installs the 2 KiB-aligned
   `exception_vectors_el1` table in `VBAR_EL1`.
7. The primary CPU calls `kernel_main`.
8. At EL1, kernel initialization parses the FDT, establishes the current
   QEMU-specific memory map, initializes the physical and virtual page
   allocators, and enables the initial EL1 MMU map.

## Exception and interrupt handling

- The 16 EL1 vector slots are aligned and named by architectural slot.
- Current-EL SPx IRQ uses a dedicated entry path. It saves x0-x30,
  ELR_EL1, and SPSR_EL1 in a 272-byte, 16-byte-aligned frame before calling
  `nova_irq_dispatch`.
- The dispatcher acknowledges GICv2, accounts for the physical timer PPI
  (interrupt ID 30), rearms the EL1 generic timer, and issues EOI before a
  possible task handoff. It returns the frame address that exception assembly
  must restore.
- Timer-driven preemption is enabled for the single-core prototype after a
  five-tick quantum. The selected task is resumed through a full exception
  frame and ERET; cooperative task switches use the separate AAPCS64 context.
- Synchronous exceptions, FIQ, SError, and unsupported vector slots use a
  terminal diagnostic path. The diagnostic identifies the architectural
  vector class (synchronous, IRQ, FIQ, or SError), decodes known ESR_EL1
  exception classes, reports ELR_EL1, FAR_EL1, and SPSR_EL1, then halts.
  This is typed diagnosis, not exception recovery or per-class service.
- A separate `NOVA_TEST_EXCEPTION` image executes `SVC #0` to verify the
  synchronous EL1 vector path in QEMU.
- Dedicated MMU-fault images verify both an unmapped-address data abort and a
  task-stack guard-page data abort.

## Important constraints

- The normal kernel boot image is still tested through the QEMU path that
  starts at EL1. A separate secure QEMU test image now verifies the narrow
  EL3-to-EL2-to-EL1 transition sequence. This is not yet a general firmware
  handoff contract for arbitrary boards or bootloaders.
- EL2/EL3 vector tables, trap handling, and recovery policy are not
  implemented. The transition test validates the controlled entry sequence,
  not a complete hypervisor or secure-monitor environment.
- The bootstrap stack is reserved inside `.bss`. The assembly entry does not
  push data or call another function until the clearing loop has completed.
- Only the primary CPU clears the shared `.bss` range. Secondary CPUs park
  before touching that range.
- The current MMU map and GICv2/timer integration are specific to QEMU
  `virt`; this is not a general FDT-driven page-table builder.
- The kernel remains single-core. There are no per-CPU scheduler stacks,
  SMP locks, or remote TLB shootdowns.
- User-mode execution, per-process address spaces, FP/SIMD task context, FIQ
  scheduling, and SError recovery are not implemented.
- CI validates the configured QEMU `virt` path. It does not establish
  compatibility with a physical phone or arbitrary boards.

## Next boot milestones

- Extend the tested EL2-to-EL1 entry policy to documented firmware handoffs,
  and add EL2/EL3 vector tables plus trap handling.
- Add typed synchronous, FIQ, and SError handlers with explicit recovery or
  terminal-fault policies.
- Pin and document a supported QEMU/toolchain version matrix.
- Validate the boot contract on a second virtual platform or documented
  physical development board.
- Add per-CPU state before enabling secondary processors.
