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
syscall number and x0-x5 as arguments; x0 is the return value. The initial
syscall set is limited to write to the existing diagnostic UART and exit.
All pointers and lengths must be validated against the current address space
before dereference. No user pointer may be passed directly to an unbounded
kernel string routine. Unknown syscall numbers return a defined negative
error and do not panic the kernel.

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
   EL0t with interrupts masked until the final controlled return, and SP_EL0
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