# Nova OS — Roadmap

A milestone is complete only when its acceptance criteria are demonstrated and recorded.

## Milestone 0 — Project foundation

Status: complete.

- [x] Define project vision and product principles.
- [x] Establish an initial repository structure.
- [x] Document the build and test workflow.
- [x] Add an automated AArch64 build and QEMU smoke test.

Exit criteria met: the repository has a documented build path and an automated QEMU serial boot check. This milestone does not imply a complete operating system.

## Milestone 1 — Virtual platform bring-up

Status: in progress.

- [x] Document the QEMU `virt` machine configuration.
- [x] Establish and document the current experimental early-boot sequence.
- [x] Verify PL011 UART output in QEMU.
- [x] Add ELF header and linker-symbol inspection to CI.
- [x] Add a runtime probe for zero-initialized `.bss`.
- [x] Report the current exception level over UART.
- [ ] Pin and document a supported QEMU/toolchain version set.
- [ ] Validate the boot contract against a second virtual platform or a documented physical development board.

Exit criteria: repeatable QEMU boot with captured evidence and no reliance on undocumented local state. The current prototype remains QEMU-specific.

## Milestone 2 — Kernel foundations

Status: in progress.

Implemented and QEMU-tested:
- [x] Add a 2 KiB-aligned EL1 exception vector table.
- [x] Install the EL1 vector table when the boot path reports EL1.
- [x] Capture `ESR_EL1` and `ELR_EL1` in a terminal diagnostic handler.
- [x] Build a separate controlled-`SVC` test image and verify the synchronous exception path in CI.

Remaining:
- [ ] Distinguish synchronous exceptions, IRQ, FIQ, and SError with typed handlers.
- [ ] Capture a complete exception register frame and document its layout.
- [ ] Define an explicit EL2 boot/transition policy and EL2 vector handling.
- [ ] Define a platform memory map and physical memory allocator.
- [ ] Establish timer and interrupt-controller handling.
- [ ] Introduce a minimal scheduler only after interrupt and timer foundations are tested.
- [ ] Add host-side tests for portable components where practical.

Exit criteria: documented memory, exception, timer, and scheduling behavior under the virtual platform.

## Milestone 3 — System service model

- Define process and service boundaries.
- Design capability-based access control.
- Define IPC and service lifecycle.
- Specify logging, crash reporting, and recovery behavior.

Exit criteria: architecture documents and testable service interfaces.

## Milestone 4 — Device and user interface foundations

- Select a supported development board only after evaluating boot documentation and driver availability.
- Prototype display, input, storage, networking, and power interfaces.
- Build a minimal graphical shell.

Exit criteria: a documented reference device can boot and provide basic interactive I/O.

## Milestone 5 — AI interaction layer

- Define a task representation and execution state machine.
- Build a permission and confirmation model.
- Add tool adapters behind explicit capability boundaries.
- Implement cancellation, audit history, retries, and recovery.
- Evaluate task completion on a versioned benchmark.

Exit criteria: repeatable cross-service task demonstrations with measurable success and no unauthorized side effects.

## Milestone 6 — Application ecosystem and hardening

- Define application packaging and lifecycle.
- Implement secure updates and rollback.
- Conduct threat modeling and security testing.
- Test accessibility, localization, performance, battery use, and reliability.

Exit criteria: a clearly scoped developer preview with documented limitations.

## Release discipline

Do not mark a feature complete because source code exists. Use these states consistently:
- **Designed:** documented, not implemented.
- **Implemented:** code exists.
- **Build-verified:** compiler/linker completed successfully.
- **Runtime-verified:** behavior observed in the target runtime.
- **Hardware-verified:** tested on the named physical device.
