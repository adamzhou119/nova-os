# ADR 0007: Scheduler Execution State and IRQ-Safe Handoff

- Status: Proposed
- Date: 2026-10-31
- Scope: Single-core AArch64 EL1 scheduler handoff

## Context

The IRQ frame ABI and per-task IRQ-frame bookkeeping are now present and tested. Actual timer preemption is still disabled. The cooperative scheduler has a separate AAPCS64 context for the scheduler loop and each task, while a timer interrupt captures a full exception frame on the currently active stack.

The scheduler currently uses `scheduler_active` and `current_task_id` to decide whether timer ticks should count toward a task quantum. Those two fields alone do not express every transition boundary. In particular, a timer IRQ can arrive after a cooperative context switch has returned to the scheduler context but before the scheduler loop clears `current_task_id`. Treating that interval as task execution could incorrectly save a scheduler-stack IRQ frame as if it belonged to a task.

## Decision

Do not enable timer-driven context switching until scheduler execution ownership is explicit and all ownership changes are atomic with respect to local IRQ delivery.

Introduce an explicit execution phase with at least these states:

- `INACTIVE`: scheduler has not started or has fully returned.
- `SCHEDULER`: CPU is executing the scheduler loop on `scheduler_context`.
- `TASK`: CPU is executing a task context and `current_task_id` identifies its RUNNING TCB.

The IRQ dispatcher may request a task switch only when the phase is `TASK`, the current ID resolves to a RUNNING task, and the interrupt is the configured physical timer interrupt.

### Cooperative handoff

The task-to-scheduler handoff must be a short local-IRQ critical section:

1. Save the current DAIF state and mask IRQ delivery.
2. Publish the phase as `SCHEDULER` before switching stacks.
3. Perform the cooperative context switch.
4. When the task is later resumed, restore the saved DAIF state before returning to task code.

The scheduler loop must publish `TASK` and the selected task ID as one protected transition before switching into a task. It must publish `SCHEDULER` before processing the task's return to the scheduler context. The implementation must not assume C statement ordering alone provides interrupt atomicity.

The exact assembly/C boundary for preserving DAIF must be chosen so the saved mask state belongs to the resumed context, not accidentally to whichever context happened to execute the restore instruction.

### IRQ handoff

Once the phase and ownership invariants are enforced, the timer IRQ path may:

1. Save the incoming frame pointer only to the current RUNNING task.
2. Mark that task READY when another READY task is selected.
3. Select a READY task and validate that its saved frame belongs to its mapped stack.
4. Mark the selected task RUNNING, publish its ID and TASK phase, reset the quantum, and return its frame pointer.
5. If no different task is ready, retain the current task and reset its quantum without changing frame ownership.

The dispatcher must never use a cooperative `nova_cpu_context` as an IRQ return frame.

### Task exit and reclamation

A returning task is marked ZOMBIE before handoff. Its stack remains allocated until execution is safely on the scheduler stack. Destruction and stack reclamation must occur only from a context that is not using the target task's stack.

## Required tests before enabling preemption

- Host-test phase transitions and rejection of IRQ scheduling while phase is INACTIVE or SCHEDULER.
- Host-test that a scheduler-stack frame cannot be recorded as a task frame.
- Host-test round-robin selection and no-alternative-ready behavior, including frame ownership.
- QEMU two-task CPU-bound test: neither task calls Yield, and both make progress through timer-driven frame switches.
- QEMU task-exit test proving a ZOMBIE task is never selected and its stack is reclaimed only from scheduler context.
- Full existing CI suite, including exception, MMU-fault, and ELF checks.

## Consequences

This design intentionally adds a small amount of explicit scheduler state before enabling preemption. It avoids relying on timing-sensitive assumptions around a cooperative context switch. The first implementation remains single-core, integer-register-only, and QEMU virt/GICv2-specific.
