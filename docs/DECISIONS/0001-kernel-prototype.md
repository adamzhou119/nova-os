# ADR 0001: Start with a small AArch64 virtual kernel prototype

- Status: accepted provisionally
- Date: 2026-10-31

## Context

The repository needs a small, testable technical milestone before committing to phone hardware or a complete mobile platform. A virtual AArch64 target allows early experiments without requiring a physical development board.

## Decision

Start with a minimal freestanding AArch64 ELF targeting QEMU's `virt` machine. The first experiment provides an entry point and PL011 serial output. Keep product-level AI orchestration and system-service design separate from the kernel bring-up work.

## Consequences

Positive:
- Early work can be compiled and inspected without a phone.
- The serial console provides a simple observable boot signal.
- The prototype can be exercised in CI once the QEMU path is verified.

Trade-offs:
- Bare-metal boot assumptions must be validated against the selected QEMU version and boot protocol.
- This prototype does not establish a viable production mobile architecture.
- Hardware drivers, power management, graphics, security isolation, and application compatibility remain future work.

## Revisit when

Revisit this decision after a reproducible QEMU boot is established and a comparison of existing kernels (for example, adapting an existing kernel versus continuing from scratch) has been documented.
