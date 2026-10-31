# Nova OS — Roadmap

A milestone is complete only when its acceptance criteria are demonstrated and recorded.

## Milestone 0 — Project foundation

Status: in progress.

- [x] Define project vision and product principles.
- [x] Establish an initial repository structure.
- [ ] Add a reproducible build environment and pinned toolchain guidance.
- [ ] Run the kernel under QEMU and capture actual serial output.
- [ ] Add a minimal automated build check.

Exit criteria: a clean checkout can be built with documented commands, and a QEMU boot test produces the expected serial output. Until then, the prototype is not described as boot-tested.

## Milestone 1 — Virtual platform bring-up

- Document the supported QEMU version and machine configuration.
- Establish the AArch64 boot contract and exception-level assumptions.
- Verify UART output in QEMU.
- Add linker-map and ELF inspection steps.
- Add a smoke test to CI.

Exit criteria: repeatable QEMU boot with captured evidence and no reliance on undocumented local state.

## Milestone 2 — Kernel foundations

- Define memory map and physical memory allocator.
- Add exception vectors and basic fault diagnostics.
- Establish timer and interrupt handling.
- Introduce a minimal scheduler only after interrupt and timer foundations are tested.
- Add host-side tests for portable components where practical.

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
