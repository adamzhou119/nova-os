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
7. When running at EL1, the entry code installs the 2 KiB-aligned
   `exception_vectors_el1` table in `VBAR_EL1`.
8. The primary CPU calls `kernel_main`.

## Exception handling status

- All 16 architectural EL1 vector slots currently branch to one terminal
  diagnostic handler.
- Each vector slot passes its architectural slot index to a common terminal handler. The handler identifies the vector class and reports `ESR_EL1` and `ELR_EL1` over UART, then halts.
- The current handler does not restore the interrupted context or return from
  exceptions. IRQ/FIQ routing and interrupt-controller integration are not
  implemented.
- A separate test image is built with `NOVA_TEST_EXCEPTION`; it executes
  `SVC #0` so CI can verify the synchronous EL1 vector path in QEMU.

## Important constraints

- The stack is currently reserved inside `.bss`. The assembly entry does not
  push data or call another function until the clearing loop has completed.
- Only the primary CPU clears the shared `.bss` range. Secondary CPUs park
  before touching that range.
- The linker aligns the end of `.bss` to an 8-byte boundary because the
  clearing loop writes 8 bytes at a time.
- Exception vectors are installed only at EL1. The prototype reports other
  exception levels but does not yet implement an EL2 vector table or EL2-to-EL1
  transition policy.
- There is no MMU setup, cache policy, interrupt controller setup, or device-tree
  parser yet.
- The QEMU CI smoke test validates the configured QEMU `virt` path only. It
  does not establish compatibility with a physical phone or arbitrary boards.

## Next boot milestones

- Define an explicit EL2-to-EL1 policy where required by the selected boot path.
- Split synchronous faults, IRQ, FIQ, and SError into typed handlers.
- Add safe register-frame capture and a documented exception context layout.
- Introduce a platform memory map before enabling an allocator or MMU.
- Add per-CPU state before enabling secondary processors.
