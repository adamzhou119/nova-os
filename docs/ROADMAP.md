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

Implemented and runtime-verified on QEMU virt:
- [x] Add a 2 KiB-aligned EL1 exception vector table.
- [x] Install the EL1 vector table when the boot path reports EL1.
- [x] Capture synchronous exception diagnostics including ESR_EL1, ELR_EL1, FAR_EL1, and SPSR_EL1.
- [x] Capture and restore the complete integer IRQ frame (x0-x30, ELR_EL1, SPSR_EL1).
- [x] Add a controlled SVC exception image and verify the synchronous exception path in CI.
- [x] Parse FDT RAM and reserved ranges and build a firmware-backed physical page allocator.
- [x] Add a bounded kernel virtual-page allocator, 4 KiB map/unmap, permission checks, alias W^X tracking, and frame pinning.
- [x] Add EL1 generic timer and QEMU virt GICv2 timer-PPI handling.
- [x] Add task lifecycle management, one-page kernel stacks, and deferred ZOMBIE reclamation on the scheduler stack.
- [x] Add cooperative task switching and timer-driven IRQ preemption with distinct FIRST_RUN, COOPERATIVE, IRQ, and ACTIVE resume paths.
- [x] Validate mixed cooperative/IRQ scheduling and repeated timer preemption between two CPU-bound tasks that never call Yield or Block.
- [x] Add host-side allocator, task, scheduler, and IRQ-dispatcher tests.

Remaining:
- [ ] Distinguish synchronous exceptions, IRQ, FIQ, and SError with typed handlers and documented recovery policy.
- [ ] Define an explicit EL2 boot/transition policy and EL2 vector handling.
- [ ] Pin and document the supported QEMU/toolchain matrix.
- [ ] Generalize device-memory mappings and page-table construction from FDT data.
- [ ] Define per-task address-space and user-mode context requirements before introducing unprivileged tasks.
- [ ] Evaluate SMP only after single-core synchronization and ownership invariants are documented.

Exit criteria: documented memory, exception, timer, and scheduling behavior under the virtual platform, with repeatable host and QEMU evidence. The current prototype remains single-core and QEMU-specific.

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
