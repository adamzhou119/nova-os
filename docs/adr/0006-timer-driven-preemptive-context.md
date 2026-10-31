# ADR 0006: Timer-Driven Preemptive Task Context

- Status: Proposed
- Date: 2026-10-31
- Scope: Single-core AArch64 EL1 kernel on QEMU virt / GICv2

## Context

Nova currently has two distinct context mechanisms:

1. The cooperative AAPCS64 context saves x19-x30 and SP. It is sufficient when a task calls the scheduler through an ordinary function call, because the compiler preserves callee-saved registers.
2. The IRQ exception frame saves x0-x30, ELR_EL1, and SPSR_EL1 in a 272-byte, 16-byte-aligned frame. The IRQ dispatcher currently returns the interrupted frame unchanged.

The timer IRQ increments a five-tick quantum counter and raises a deferred preemption request. It does not yet select another task. Treating the cooperative context as an IRQ context would lose caller-saved registers and the interrupted exception-return state.

## Decision

Preemptive scheduling will use a per-task exception-frame pointer, separate from the cooperative AAPCS64 context during the initial integration.

The frame layout is the existing assembly ABI:

| Offset | Field |
| ---: | --- |
| 0..240 | x0..x30 (8 bytes each) |
| 248 | ELR_EL1 |
| 256 | SPSR_EL1 |
| 264 | reserved padding |
| 272 | End of frame |

The frame remains 16-byte aligned. C declarations must statically assert the total size and key offsets against `exceptions.S`.

### Task creation

Task creation will construct an initial exception frame near the top of its mapped kernel stack. The initial frame will:

- Set x0 to the task argument.
- Set the callee-saved trampoline inputs to the task entry, argument, and TCB pointer.
- Set ELR_EL1 to the first-run trampoline.
- Set SPSR_EL1 to EL1h with the intended interrupt mask state.
- Be retained in the task control block until the task first runs.

The stack allocation must remain alive while the task is READY, RUNNING, or has a saved exception frame.

### IRQ dispatch

The IRQ dispatcher will:

1. Acknowledge the GIC interrupt.
2. Account for the timer tick and re-arm the physical timer.
3. Complete the interrupt with EOI before changing task ownership.
4. If the quantum has expired and preemption is enabled, save the interrupted frame pointer in the current task.
5. Select the next READY task using round-robin task-ID ordering.
6. Update task states and the scheduler's current-task ID atomically with respect to local IRQ delivery.
7. Return the selected task's saved frame pointer to the assembly restore path.

If there is no other READY task, the current frame is returned unchanged and its quantum accounting is reset. Non-timer IRQs must not cause a scheduling decision unless a separately documented wakeup policy is introduced.

### Task exit

A task that returns from its entry function must become ZOMBIE and never be selected again. Its stack cannot be freed while the CPU is still executing on it; reclamation must occur later from another task or scheduler context.

### Concurrency and architecture limits

This first implementation is single-core. Scheduler state transitions and IRQ frame ownership must be protected against nested local IRQs. SMP locking, remote TLB shootdowns, user-mode contexts, and SIMD/FP context preservation are explicitly out of scope. Kernel code remains built with `-mgeneral-regs-only`, and that restriction must remain documented until FP/SIMD state is saved.

## Rejected approach

Calling the existing `nova_arch_context_switch()` directly from the C IRQ dispatcher is rejected. That routine saves only x19-x30 and SP and returns through a normal `ret`; it cannot preserve x0-x18, ELR_EL1, or SPSR_EL1 from an interrupted instruction stream.

## Required validation before enabling preemption

- Host tests for exception-frame size, offsets, initial-frame construction, scheduler state transitions, and round-robin frame selection.
- QEMU test with at least two CPU-bound tasks that never call Yield.
- Verify both tasks make progress across timer IRQs and resume at their interrupted instruction streams.
- Verify task exit, no-ready-task behavior, spurious IRQ handling, and that each selected frame belongs to a live task stack.
- Run the complete existing host suite, ELF inspection, QEMU boot smoke test, controlled exception test, and MMU fault test in CI.

Preemption must remain disabled until all required QEMU and CI checks pass.

## Scheduler continuation invariant

The initial scheduler implementation has one saved cooperative scheduler context. An IRQ-selected task may later call yield, block, or exit and return through that same saved scheduler continuation. Therefore the continuation must not assume that the task which originally entered it is the task that most recently returned.

Before IRQ frame switching can be integrated, the scheduler dispatch loop must:

- Treat the scheduler continuation as a shared dispatch point, and identify the returning task from the authoritative current-task ID rather than a task ID kept in a suspended C local.
- Reap only the task that actually returned to the scheduler and only after execution is on the scheduler stack.
- Select the next READY task by resume kind: FIRST_RUN and IRQ tasks must enter through the exception-frame restore path; COOPERATIVE tasks must enter through the AAPCS64 context-switch path.
- Preserve a single, well-defined scheduler-stack continuation when tasks alternate between IRQ-frame entry and cooperative yield/exit.
- Keep the current-task ID, task states, and selected frame ownership consistent before an exception return or cooperative context switch.

A unit test must model a task entered from an IRQ frame yielding back to the shared scheduler continuation, followed by resumption of the previously interrupted task. The QEMU CPU-bound-task test remains the acceptance test. This section is a design constraint, not evidence that the integration is implemented.
