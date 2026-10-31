# ADR 0006: Timer-Driven Preemptive Task Context

- Status: Accepted
- Date: 2026-10-31
- Scope: Single-core AArch64 EL1 kernel on QEMU virt / GICv2

## Context

Nova uses two distinct task-resumption mechanisms:

1. Cooperative AAPCS64 contexts save x19-x30 and SP. They are used when a task calls Yield, Block, or Exit through an ordinary function call.
2. IRQ exception frames save x0-x30, ELR_EL1, and SPSR_EL1 in a 272-byte, 16-byte-aligned frame. They preserve the exact interrupted instruction stream and PSTATE.

These frames are not interchangeable. A cooperative context cannot restore caller-saved registers or exception-return state. An IRQ frame must not be resumed through a normal `ret` context switch.

## Decision

Preemptive scheduling uses the per-task IRQ-frame pointer separately from the cooperative AAPCS64 context.

### Exception-frame ABI

| Offset | Field |
| ---: | --- |
| 0..240 | x0..x30 (8 bytes each) |
| 248 | ELR_EL1 |
| 256 | SPSR_EL1 |
| 264 | reserved padding |
| 272 | End of frame |

The C structure statically asserts the total size and key offsets. IRQ entry preserves the interrupted x16 value and maintains 16-byte stack alignment.

### Task creation and resume kinds

Each task has one authoritative resume kind:

- `FIRST_RUN`: a newly created task. The TCB contains a synthetic initial IRQ frame and an initialized cooperative context.
- `COOPERATIVE`: a task suspended by Yield or Block; it resumes only through `nova_arch_context_switch`.
- `IRQ`: a task interrupted by a timer IRQ; it resumes only through an exception frame and ERET.
- `ACTIVE`: a running task; no saved IRQ frame is available for reuse.

Task creation builds the synthetic frame at the top of its mapped kernel stack. It sets x0 to the task argument, x19/x20/x21 to the trampoline inputs, ELR_EL1 to the first-run trampoline, and SPSR_EL1 to EL1h with IRQs enabled on exception return.

### Timer IRQ handoff

The timer IRQ path:

1. Acknowledges the GIC interrupt.
2. Accounts for the timer tick and rearms the physical timer.
3. Issues EOI before changing task ownership.
4. Requests a handoff only when the scheduler phase is TASK, a RUNNING current task exists, and the five-tick quantum has expired.
5. Saves the interrupted frame in the current task and selects the next READY FIRST_RUN/IRQ task in round-robin ID order.
6. Atomically marks the outgoing task READY/IRQ and the selected task RUNNING/ACTIVE, updates the current task ID, and returns the selected frame pointer to exception assembly.

If there is no alternative IRQ-returnable task, the current frame is returned unchanged and its quantum is reset. Non-timer IRQs do not trigger task scheduling.

### Restoring an IRQ task from the scheduler

The scheduler scans all READY tasks in round-robin order and dispatches by resume kind:

- FIRST_RUN and COOPERATIVE tasks use their AAPCS64 context.
- IRQ tasks are atomically consumed by `nova_task_take_irq_context`, which validates stack ownership/alignment, changes READY/IRQ to RUNNING/ACTIVE, and clears the stored frame pointer.
- `nova_arch_irq_resume` switches to the selected task stack, restores x0-x30, ELR_EL1, and SPSR_EL1, then executes ERET. It never returns through the normal C call path.

An IRQ-resumed task that later yields, blocks, or exits saves its cooperative continuation and returns to the shared scheduler context. The scheduler identifies the task that actually returned using the authoritative current-task ID, not a stale local from the original dispatch. Task stacks are reaped only after execution is on the scheduler stack.

Wakeups are idempotent for READY tasks. A task may already be READY because a timer IRQ preempted it before another task attempts to wake it.

## Validation

The following checks are automated in GitHub Actions:

- Host tests for exception-frame layout, initial-frame construction, frame ownership/alignment, resume-kind transitions, one-time IRQ-frame consumption, task lifecycle, and scheduler handoff.
- Host IRQ-dispatcher tests proving timer IRQs can return a selected frame pointer and non-timer/spurious interrupts do not request handoff.
- QEMU mixed-mode test: an IRQ-preempted task resumes, blocks cooperatively, is woken, and exits; another task yields and exits.
- QEMU CPU-bound test: two tasks never call Yield or Block. Both make progress across repeated timer IRQ handoffs, resume at their interrupted instruction streams, and are reaped.
- Full CI also runs ELF inspection, normal QEMU boot, controlled SVC exception, and unmapped-address MMU fault tests.

Latest recorded full CI success: workflow run 41197964595, commit 0ea1fca.

## Consequences and limits

This implementation is single-core and specific to the QEMU virt/GICv2 platform. SMP locking, remote TLB shootdowns, user-mode contexts, FIQ/SError scheduling, FP/SIMD state, and per-process address spaces are out of scope. Kernel code is compiled with `-mgeneral-regs-only`; that restriction must remain until FP/SIMD state is explicitly saved and restored.
