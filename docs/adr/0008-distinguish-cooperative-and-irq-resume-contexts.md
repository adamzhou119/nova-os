# ADR 0008: Distinguish Cooperative and IRQ Resume Contexts

- Status: Proposed
- Date: 2026-10-31
- Scope: Single-core AArch64 task resumption

## Context

Nova OS now has two independent ways to suspend a task:

1. Cooperative suspension through `nova_arch_context_switch`. This saves the AAPCS64 callee-saved registers and stack pointer in `nova_cpu_context`.
2. Asynchronous suspension through an exception vector. This saves x0-x30, ELR_EL1, and SPSR_EL1 in a stack-resident `nova_irq_frame`.

These frames are not interchangeable. A cooperative task that called Yield has a valid cooperative context on its stack, but its `irq_frame` may still point to the synthetic first-run frame or to an old IRQ frame that has already been consumed by ERET. Conversely, a task preempted by an IRQ has a live IRQ frame, but its cooperative context may only describe an earlier yield point.

The current task control block exposes both pointers but does not yet identify which one is authoritative for the next resume.

## Decision

Do not implement IRQ-driven task selection using only task state or a non-null `irq_frame` pointer. Add an explicit resume-kind invariant before enabling preemption.

The minimum resume kinds are:

- `NOVA_TASK_RESUME_COOPERATIVE`: resume only through `nova_arch_context_switch` using `nova_cpu_context`.
- `NOVA_TASK_RESUME_IRQ`: resume only by returning the task's live `nova_irq_frame` from the IRQ dispatcher.
- `NOVA_TASK_RESUME_FIRST_RUN`: resume using the synthetic initial IRQ frame built at task creation.

Transitions must be explicit:
- Task creation sets FIRST_RUN.
- First dispatch consumes FIRST_RUN and enters the trampoline.
- A cooperative Yield or block records COOPERATIVE.
- A timer IRQ that actually suspends a running task records IRQ and stores the current frame pointer.
- Returning from an IRQ without switching does not publish the frame as a resumable IRQ context, because ERET consumes it.

The scheduler must not attempt to resume a COOPERATIVE task by returning its stale IRQ frame. The initial implementation may continue using the cooperative scheduler loop for cooperative-ready tasks and limit IRQ frame selection to tasks whose resume kind is FIRST_RUN or IRQ, provided round-robin fairness and task-state transitions are defined and tested.

## Required tests

- Host tests for all legal resume-kind transitions and rejection of illegal transitions.
- Host tests proving cooperative-ready tasks are not selected as IRQ return frames.
- Host tests proving FIRST_RUN and IRQ tasks have aligned, in-stack frames.
- QEMU test that preempts a task, switches away, then later resumes its saved IRQ frame.
- QEMU test that a task yields cooperatively, is selected again, and resumes through its cooperative context rather than its stale IRQ frame.
- Mixed-mode QEMU test with at least two CPU-bound tasks and one yielding task.
- Verify task exit and stack reclamation remain scheduler-stack-only.

## Consequences

This adds state to the task control block and scheduler, but makes the resume path explicit and reviewable. It prevents a subtle corruption class where a valid-looking but already-consumed exception frame is used as a task's next program counter and stack state.

Timer-driven context switching remains disabled until this invariant is implemented and tested.
