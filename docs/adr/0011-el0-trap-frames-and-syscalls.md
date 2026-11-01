# ADR 0011: EL0 Trap Frames, SVC, and User-Task Fault Containment

- Status: Proposed implementation design
- Date: 2026-10-31
- Scope: Single-core AArch64, QEMU virt, EL1 kernel with EL0 AArch64 tasks

## Context

Nova already has private address-space roots, 8-bit ASIDs, EL0 page mappings,
permission transitions, and TTBR0 handoff during scheduling. Tasks still enter
through an EL1 trampoline. The current 272-byte IRQ frame preserves x0-x30,
ELR_EL1, and SPSR_EL1, but it does not preserve SP_EL0. Lower-EL synchronous
exceptions still reach the terminal vector handler. Therefore the current
scheduler and address-space tests do not demonstrate EL0 execution.

EL0 support must not reuse the current frame by assumption: a user task's
stack pointer is SP_EL0 while exception code executes on the privileged
SP_EL1 stack. The user SP must be saved before C code or scheduling can change
which task is current.

## Decision

Introduce a distinct, explicitly versioned user trap-frame contract before
enabling user tasks. The initial full user frame is 288 bytes and 16-byte
aligned:

| Offset | Field |
| ---: | --- |
| 0..240 | x0..x30, 8 bytes each |
| 248 | ELR_EL1 |
| 256 | SPSR_EL1 |
| 264 | SP_EL0 |
| 272 | ESR_EL1 |
| 280 | FAR_EL1 |

The assembly layout and C structure must have compile-time size/offset
assertions. Lower-EL AArch64 synchronous vector slot 8 must branch directly
to a dedicated entry stub without first borrowing x16 (all user GPRs must be
preserved). That stub saves the complete frame on the current task's EL1
stack, captures SP_EL0/ESR_EL1/FAR_EL1, and calls a C dispatcher. Lower-EL IRQ
continues to use the IRQ entry path, but that path must also save SP_EL0 and
restore the selected task's SP_EL0 before ERET. Current-EL exceptions remain
kernel-fatal until a separate kernel exception recovery design exists.

The dispatcher uses the saved SPSR mode and vector slot, not a caller-provided
task ID, to classify origin. Only a synchronous exception from EL0 may enter
the user policy:
- AArch64 SVC (EC 0x15) enters a bounded syscall dispatcher.
- Supported user-originated instruction/data aborts and explicitly supported
  debug traps terminate the current task with a recorded fault reason.
- Kernel-originated exceptions, SError, unsupported exception classes, and
  malformed frames remain fatal.

The first syscall ABI is intentionally tiny and versioned. It uses x8 as the
syscall number and x0-x5 as arguments; x0 is the return value. The bootstrap syscall set is deliberately narrower than the eventual
service ABI: syscall 0 returns the current task ID for bring-up verification,
and syscall 1 exits the current task with the value in x0. A bounded UART
write syscall is deferred until user-pointer and length validation exist.
No user pointer may be passed directly to an unbounded kernel string routine.
Unknown syscall numbers return a defined negative error and do not panic the
kernel.

## Entry and scheduling invariants

1. User code, user stack, and syscall data must be mapped in the task's
   address space with explicit EL0 permissions. Kernel text/data, device
   mappings, task control blocks, and kernel stacks remain inaccessible.
2. Each task keeps its own kernel stack. Every exception from EL0 arrives on
   the privileged stack configured by the boot/runtime exception-level setup;
   saved SP_EL0 is task state, not the C dispatcher stack.
3. The complete user frame is the resumable context for first-run, IRQ, and
   synchronous return. Never resume a user frame through nova_cpu_context.
4. Before ERET to a user frame, the scheduler must activate the target
   address space and install its TTBR0/ASID. The kernel stack remains
   reachable through the shared privileged mapping during handoff.
5. A faulted task is marked ZOMBIE while IRQs are masked, but its kernel
   stack, user mappings, and address-space reference are reclaimed only
   after execution has returned to the scheduler stack.
6. The EL0 entry path sets ELR_EL1 to the validated user entry, SPSR_EL1 to
   EL0t with the explicitly selected DAIF mask state, and SP_EL0
   to the top of the validated user stack. It must not return to EL0 before
   all frame fields and address-space state are initialized.

## Implementation sequence

1. Add the frame structure, assembly offsets, and host layout tests without
   enabling EL0 entry.
2. Add a lower-EL synchronous vector entry and a test-only dispatch path that
   proves the frame captures registers, SP_EL0, ESR, and FAR.
3. Add a safe EL0 first-run frame and ERET trampoline; initially run a tiny
   user payload that executes SVC and exits.
4. Add the bounded syscall ABI and pointer validation.
5. Add user fault containment and deferred resource reclamation.
6. Add QEMU tests for user-mode execution, SVC round-trip, timer preemption
   between distinct address spaces, invalid user read/write/execute, and
   proof that a user fault does not halt a healthy peer task.

## Explicit non-goals

This design does not define POSIX, ELF loading, dynamic linking, filesystems,
signals, shared memory, SMP, or a general-purpose driver ABI. It does not
claim EL0 support until the QEMU tests above pass. Hardware testing remains
deferred.

## Implementation status

The initial 288-byte C frame and host layout test are present. The EL1 lower-EL AArch64 synchronous vector saves x0-x30, ELR_EL1, SPSR_EL1, SP_EL0, ESR_EL1, and FAR_EL1 before calling C. The lower-EL IRQ path preserves SP_EL0 across timer preemption. A user task now enters through a kernel-stack trampoline that first saves the scheduler continuation, allowing a later SVC exit to return to the scheduler stack before task-stack reclamation.

The host scheduler test now seeds distinct SP_EL0 values in two task frames and verifies that cross-address-space IRQ handoff selects each task with its own saved user stack pointer intact.

The lower-EL AArch64 IRQ vector slot now shares the common IRQ entry and scheduler handoff path. Together with the SP_EL0 field, this preserves the user stack across timer IRQs; actual EL0 first-run dispatch remains a separate, unfinished step.

`nova_task_create_user()` now allocates a separate guarded kernel stack, builds the initial 272-byte ERET frame, retains the requested address-space reference, and publishes the task as `USER_FIRST_RUN`. The scheduler dispatches this resume kind through a kernel-stack trampoline: it first saves the scheduler continuation, then restores the prepared user IRQ frame with ERET. Host task tests verify that the user task is excluded from ordinary cooperative selection and that the initial frame is consumed once. The frame initializer now rejects entry/stack addresses outside the reserved EL0 virtual-address window, as well as misaligned addresses. The API still requires the caller to map the entry executable and user stack writable with EL0 permissions before task creation; it does not independently prove those mappings. A minimal SVC ABI validates EL0t origin and EC 0x15. Syscall 0 returns the current task ID in x0 and restores the full 288-byte user frame with ERET. For AArch64 SVC, ELR_EL1 already contains the address of the instruction following SVC, so the dispatcher must not add another four bytes; doing so skips the first user instruction after the syscall. Syscall 1 records the exit code and switches to the saved scheduler continuation; task reclamation occurs only after execution is back on the scheduler stack. Unknown calls return -ENOSYS. A dedicated QEMU image now runs a small EL0 payload that invokes syscall 0, writes the returned task ID to a user-writable page, and exits through syscall 1. The runtime result is pending CI. User-pointer syscalls and fault containment remain unimplemented.
