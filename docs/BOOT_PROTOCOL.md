# Early Boot Contract

This document records the current experimental boot path for the AArch64 QEMU
prototype. It is intentionally narrower than a general firmware or phone boot
specification.

## Current sequence

1. The image enters at the linker entry symbol `_start`.
2. The primary CPU loads the address of the statically reserved 16 KiB stack.
3. The entry code clears the linker-defined `__bss_start..`__bss_end range.
4. A data synchronization barrier and instruction synchronization barrier
   complete the initialization sequence before C code is entered.
5. Secondary CPUs are parked; per-CPU stacks and SMP startup are not implemented.
6. The primary CPU calls `kernel_main`.

## Important constraints

- The stack is currently reserved inside `.bss`. The assembly entry does not
  push data or call another function until the clearing loop has completed.
- The linker aligns the end of `.bss` to an 8-byte boundary because the
  clearing loop writes 8 bytes at a time.
- This prototype does not yet provide a portable firmware boot protocol,
  exception-level transition, MMU setup, cache policy, interrupt controller
  setup, or device-tree parser.
- The QEMU CI smoke test validates the configured QEMU `virt` path only. It
  does not establish compatibility with a physical phone or arbitrary boards.

## Next boot milestones

- Capture and report the current exception level.
- Define an explicit EL2-to-EL1 policy where required by the selected boot path.
- Install an exception vector table and diagnostic handlers.
- Introduce a platform memory map before enabling an allocator or MMU.
- Add per-CPU state before enabling secondary processors.
