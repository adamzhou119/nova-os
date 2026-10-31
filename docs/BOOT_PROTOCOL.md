# Early Boot Contract

This document records the current experimental boot path for the AArch64 QEMU
prototype. It is intentionally narrower than a general firmware or phone boot
specification.

## Current sequence

1. The image enters at the linker entry symbol `_start`.
2. The primary CPU establishes the bootstrap stack.
3. Secondary CPUs are parked before shared initialization; per-CPU stacks and
   SMP startup are not implemented.
4. The primary CPU clears the linker-defined `__bss_start..`__bss_end range.
5. A data synchronization barrier and instruction synchronization barrier
   complete the initialization sequence before C code is entered.
6. The entry code reads `CurrentEL[3:2]` and passes the decoded exception level
   to `kernel_main`, which reports it over the PL011 UART.
7. The primary CPU calls `kernel_main`.

## Important constraints

- The stack is currently reserved inside `.bss`. The assembly entry does not
  push data or call another function until the clearing loop has completed.
- Only the primary CPU clears the shared `.bss` range. Secondary CPUs park
  before touching that range.
- The linker aligns the end of `.bss` to an 8-byte boundary because the
  clearing loop writes 8 bytes at a time.
- Reporting the current exception level is diagnostic only. The prototype
  does not yet normalize EL2 to EL1 or define a portable firmware boot protocol.
- There is no MMU setup, cache policy, interrupt controller setup, or device-tree
  parser yet.
- The QEMU CI smoke test validates the configured QEMU `virt` path only. It
  does not establish compatibility with a physical phone or arbitrary boards.

## Next boot milestones

- Define an explicit EL2-to-EL1 policy where required by the selected boot path.
- Install an exception vector table and diagnostic handlers.
- Introduce a platform memory map before enabling an allocator or MMU.
- Add per-CPU state before enabling secondary processors.
