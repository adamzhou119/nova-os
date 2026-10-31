# ADR 0008: Distinguish Cooperative and IRQ Resume Contexts

- Status: Accepted
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

The scheduler must not attempt to resume a COOPERATIVE task by returning its stale IRQ frame. The scheduler scans all READY tasks in round-robin order and dispatches according to the authoritative resume kind:

- FIRST_RUN and COOPERATIVE tasks resume through the saved AAPCS64 context.
- IRQ tasks are atomically changed from READY/IRQ to RUNNING/ACTIVE, their saved frame pointer is consumed exactly once, and a dedicated AArch64 restore routine loads x0-x30, ELR_EL1, and SPSR_EL1 before ERET.
- The timer IRQ dispatcher acknowledges the GIC interrupt before requesting a handoff. If another IRQ-returnable task is selected, it returns that task's frame pointer to the exception assembly; it never calls the cooperative context-switch routine from IRQ context.
- When an IRQ-resumed task later yields or exits, it returns to the scheduler through its saved cooperative continuation. The scheduler accounts for the task that actually returned, rather than the task originally dispatched.

Wakeups are idempotent for READY tasks. This is required because a waiter may already have become READY through timer preemption before another task attempts to wake it.

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

Timer-driven preemption is enabled for the single-core AArch64 prototype after host-side state-machine tests and the QEMU mixed-mode scheduler test validate the complete save, handoff, ERET restore, cooperative yield, and task-exit paths. Floating-point/SIMD context is not part of the current kernel ABI because kernel code is compiled with general-purpose registers only.
