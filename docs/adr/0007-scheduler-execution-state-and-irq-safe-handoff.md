# ADR 0007: Scheduler Execution State and IRQ-Safe Handoff

- Status: Accepted
- Date: 2026-10-31
- Scope: Single-core AArch64 EL1 scheduler handoff

## Context

A timer IRQ can arrive while a task is running, while the scheduler is active, or around a cooperative context switch. A boolean scheduler-active flag and a nonzero current-task ID are insufficient to distinguish those ownership states. In particular, the scheduler must never attach an IRQ frame captured on the scheduler stack to a task.

## Decision

The scheduler publishes an explicit execution phase:

- `INACTIVE`: the scheduler has not started or has fully returned.
- `SCHEDULER`: the CPU is executing scheduler code on the scheduler stack.
- `TASK`: the CPU is executing a task and `current_task_id` identifies its RUNNING TCB.

Timer-driven handoff is permitted only in TASK phase with a nonzero current task and an expired quantum.

### Cooperative task-to-scheduler handoff

1. Save DAIF and mask local IRQ delivery.
2. Validate the current task and its context.
3. Publish the next task state and COOPERATIVE resume kind when Yield/Block is requested.
4. Reset quantum accounting and publish SCHEDULER phase before switching stacks.
5. Switch to the shared scheduler context.
6. When the same task is later resumed, restore its saved DAIF state before returning to task code.

### Scheduler-to-task dispatch

The scheduler chooses READY tasks in round-robin task-ID order. It transitions the selected task to RUNNING and publishes its ID and TASK phase while local IRQ delivery is masked.

Dispatch follows the resume-kind invariant:

- FIRST_RUN/COOPERATIVE use the AAPCS64 context-switch path.
- IRQ uses the dedicated exception-frame restore path and ERET.
- A saved IRQ frame is validated and consumed once before the restore routine is entered.

When a task returns to the scheduler, cleanup uses the authoritative current-task ID. A ZOMBIE stack is freed only after the CPU has returned to the scheduler stack. The scheduler continuation is shared across tasks; it must never rely on a suspended C local containing the ID of the task originally dispatched.

### IRQ handoff

The timer IRQ dispatcher performs timer accounting and rearms the timer, then completes the GIC interrupt with EOI. It requests scheduling only for timer interrupt ID 30. The scheduler's IRQ-masked handoff:

1. Saves the incoming frame only to the current RUNNING task.
2. Marks the outgoing task READY/IRQ.
3. Selects another READY FIRST_RUN/IRQ task.
4. Marks the selected task RUNNING/ACTIVE and clears its consumed frame pointer.
5. Updates current-task ownership and returns the selected frame pointer to exception assembly.

If no alternative task exists, the current task remains RUNNING and the original frame is returned unchanged. IRQ scheduling is rejected while the scheduler is INACTIVE or in SCHEDULER phase.

### Wakeup semantics

Waking a BLOCKED task changes it to READY. Waking an already READY task is an idempotent success: an IRQ may have made the task READY immediately before another task issues the wakeup. RUNNING, ZOMBIE, and UNUSED tasks are not made READY by this operation.

## Validation

Automated host and QEMU tests cover:

- Rejection of IRQ handoff while scheduler ownership is INACTIVE or SCHEDULER.
- Rejection of malformed frames and preservation of the quantum when no valid handoff can occur.
- Round-robin frame selection, no-alternative-ready behavior, and one-time frame consumption.
- An IRQ-resumed task yielding/blocking back to the shared scheduler continuation, followed by restoration of the previously interrupted task.
- Deferred task reclamation on the scheduler stack.
- Two CPU-bound tasks making progress through repeated timer preemption without calling Yield or Block.
- Full build, host tests, ELF checks, QEMU boot, synchronous exception, and MMU fault CI.

Latest recorded full CI success: workflow run 41197964595, commit 0ea1fca.

## Consequences and limits

The design is intentionally single-core. It does not yet provide SMP synchronization, FIQ/SError scheduling, user-mode task contexts, per-task address spaces, or FP/SIMD state preservation. The timer and GIC implementation is currently validated on QEMU virt with GICv2.
