# Nova OS — Roadmap

A milestone is complete only when its acceptance criteria are demonstrated and recorded.

## Milestone 0 — Project foundation
Status: in progress.
- [x] Define product vision and principles.
- [x] Add initial repository structure and architecture notes.
- [x] Add experimental AArch64 kernel starter.
- [ ] Pin toolchain and document reproducible build environment.
- [ ] Run under QEMU and capture actual serial output.
- [ ] Add automated build check.

Exit criteria: clean checkout builds with documented commands and QEMU boot output is captured. Until then, do not call the prototype boot-tested.

## Milestone 1 — Virtual platform bring-up
- Document QEMU version and machine configuration.
- Verify boot protocol and exception-level assumptions.
- Verify UART output.
- Add ELF/linker-map inspection and CI smoke test.

## Milestone 2 — Kernel foundations
- Define memory map and physical memory allocator.
- Add exception vectors and fault diagnostics.
- Implement timer and interrupt handling.
- Add a minimal scheduler after timer/interrupt foundations are tested.
- Test portable components where practical.

## Milestone 3 — System services
- Define process/service boundaries and IPC.
- Design capability-based access control.
- Specify service lifecycle, logging, crash reporting, and recovery.

## Milestone 4 — Device and UI foundations
- Select reference hardware based on boot documentation and driver availability.
- Prototype display, input, storage, networking, and power interfaces.
- Build a minimal graphical shell.

## Milestone 5 — AI interaction
- Define task representation and execution state machine.
- Implement permission checks and confirmation.
- Add tool adapters, cancellation, audit history, retries, and recovery.
- Evaluate against a versioned task benchmark.

## Milestone 6 — Ecosystem and hardening
- Define app packaging and lifecycle.
- Implement secure updates and rollback.
- Conduct threat modeling and security testing.
- Test accessibility, localization, performance, battery, and reliability.

## Status vocabulary
- **Designed:** documented, not implemented.
- **Implemented:** code exists.
- **Build-verified:** compiler/linker completed.
- **Runtime-verified:** behavior observed in target runtime.
- **Hardware-verified:** tested on the named physical device.
