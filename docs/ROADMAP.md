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
- [x] Pin and document the CI runner and QEMU/Clang/LLD package versions (Ubuntu 26.04 runner; Clang/LLD 21.1.6-71; QEMU 10.2.1+ds-1ubuntu3.3; exact apt package pins enforced in CI).
- [ ] Validate the boot contract against a second virtual platform or a documented physical development board.

Exit criteria: repeatable QEMU boot with captured evidence and no reliance on undocumented local state. The current prototype remains QEMU-specific.

## Milestone 2 — Kernel foundations

Status: in progress.

Implemented and runtime-verified on QEMU virt:
- [x] Add a 2 KiB-aligned EL1 exception vector table.
- [x] Install the EL1 vector table when the boot path reports EL1.
- [x] Capture synchronous exception diagnostics including ESR_EL1, ELR_EL1, FAR_EL1, and SPSR_EL1.
- [x] Capture and restore the complete integer IRQ frame (x0-x30, ELR_EL1, SPSR_EL1).
- [x] Classify all 16 EL1 vector slots as synchronous, IRQ, FIQ, or SError and decode common ESR_EL1 exception classes with host tests.
- [x] Add a controlled SVC exception image and verify the synchronous exception path in CI.
- [x] Parse FDT RAM and reserved ranges and build a firmware-backed physical page allocator.
- [x] Add a bounded kernel virtual-page allocator, 4 KiB map/unmap, permission checks, alias W^X tracking, and frame pinning.
- [x] Add EL1 generic timer and QEMU virt GICv2 timer-PPI handling.
- [x] Add task lifecycle management, guarded one-page kernel stacks, and deferred ZOMBIE reclamation on the scheduler stack.
- [x] Add cooperative task switching and timer-driven IRQ preemption with distinct FIRST_RUN, COOPERATIVE, IRQ, and ACTIVE resume paths.
- [x] Validate mixed cooperative/IRQ scheduling and repeated timer preemption between two CPU-bound tasks that never call Yield or Block.
- [x] Add host-side allocator, task, scheduler, and IRQ-dispatcher tests.

Remaining:
- [ ] Complete runtime per-class exception entry/exit and recoverable fault handling. The conservative EL0/SVC/fault policy classifier and its host tests are implemented; synchronous exceptions still halt until an EL0 frame, task-fault handoff, and syscall boundary exist.
- [x] Implement and QEMU-test a controlled EL3-to-EL2-to-EL1 transition,
      including EL1 physical timer access controls.
- [ ] Add EL2/EL3 vector tables, trap handling, and a general firmware
      handoff contract.
- [x] Pin and document the CI runner and QEMU/Clang/LLD package versions (Ubuntu 26.04 runner; Clang/LLD 21.1.6-71; QEMU 10.2.1+ds-1ubuntu3.3; exact apt package pins enforced in CI).
- [ ] Generalize device-memory mappings and page-table construction from FDT data.
- [x] Define per-task address-space ownership, mapping, permission, ASID,
      exception-frame, and isolation requirements (ADR 0010).
- [x] Add address-space metadata, unique live-ASID checks, reference counting,
      active-use protection, and task ownership/lifetime tests.
- [x] Implement private address-space page-table roots and ASID-aware TTBR0_EL1 switching; QEMU verifies shared privileged mappings and separate roots.
- [x] Implement per-address-space 4 KiB EL0 page map/unmap, in-place permission changes with break-before-make, user read/write translation checks, ASID-scoped TLB invalidation, and page-table/frame pinning.
- [x] Verify EL0 read-only enforcement and reject writable/executable aliases across address spaces in QEMU.
- [x] Switch TTBR0 and maintain address-space active references during cooperative task dispatch/return.
- [x] Allow timer IRQ-frame handoff between tasks sharing one private address space without changing TTBR0; host scheduler tests cover this path.
- [x] Implement atomic cross-address-space IRQ-frame/TTBR handoff with destination activation, TTBR0 switch, active-reference transfer, and rollback; host scheduler tests cover successful handoff and old-space deactivation failure rollback (CI runs 593–595).
- [x] Restore the previous TTBR0 and retain the active-space ownership record when releasing an address space on scheduler return fails; AArch64 build, host scheduler tests, and all QEMU checks pass in CI run 595. A dedicated fault-injection test now forces the old-space release to fail after TTBR0 switches to the kernel root; it verifies restoration of the task root, retained active ownership, and unchanged task state (CI run 598).
- [x] Verify CPU-bound tasks in distinct address spaces are preempted and resumed under QEMU virt; the serial test confirms both tasks make progress across repeated TTBR0 handoffs.
- [ ] Implement EL0 task execution, SVC boundary, and fault containment (288-byte trap frame, lower-EL sync/IRQ entry, SP_EL0 preservation, and host-tested user-task creation are implemented; first EL0 entry saves the scheduler continuation through a kernel-stack trampoline; syscall 0 returns the task ID, syscall 1 exits through deferred scheduler-stack cleanup, syscall 2 performs a 128-byte-bounded UART write after validating every covered user page and copying into a kernel buffer, and unknown calls return -ENOSYS; QEMU CI run 574 verifies EL0 data-abort and instruction-abort tasks are both reaped before a healthy peer completes; runs 560 and 562 also verify data-fault containment, valid user-buffer output, unmapped/cross-page pointer rejection, and oversized-length rejection; bounded copyin/copyout helpers are implemented and host-tested; QEMU exercises copy-in through syscall 2 and copy-out to a mapped user stack, including rejection of a write crossing into an unmapped page, along with broader user fault tests and hardware validation).
- [ ] Evaluate SMP only after single-core synchronization and ownership invariants are documented.
- [x] Add scheduler-entry fault injection for a failed destination TTBR0 install; verify the newly activated reference is released, task remains READY, and kernel TTBR0 remains active (CI pending for commit 01805f5).

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
