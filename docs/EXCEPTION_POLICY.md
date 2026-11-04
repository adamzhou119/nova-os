# Nova OS — Exception Policy

## Scope

This document defines the conservative policy boundary for EL1 synchronous
exceptions. The host-tested classifier is a decision aid, not a recovery
implementation. The current EL1 vector assembly still sends synchronous
exceptions to a terminal diagnostic handler. No EL0 task execution or syscall
dispatcher is enabled yet.

## Policy

| Origin and event | Policy classification | Required eventual action |
|---|---|---|
| EL1 synchronous exception | Kernel fatal | Capture diagnostics and stop |
| EL1 IRQ | IRQ dispatcher | Acknowledge, service, optionally schedule |
| EL0 AArch64 SVC | User SVC | Validate syscall number and arguments, then dispatch |
| EL0 instruction/data abort | User fault | Record fault and terminate only the offending task |
| EL0 breakpoint, step, watchpoint, BRK | User fault | Debug/fault policy; never silently resume |
| EL0 SError, illegal state, unknown trap | Kernel fatal | Stop until an explicit safe recovery policy exists |
| Any same-EL abort | Kernel fatal | Do not treat as a user fault |

## Safety invariants

- A trap is considered user-originated only when SPSR_EL1.M[3:0] is EL0t
  and the vector slot is the synchronous-from-lower-EL slot (slot 8).
- A same-EL abort must never be converted into task termination.
- An EL0 fault must not resume at the faulting PC without an explicit,
  validated recovery decision.
- Fault containment requires a complete lower-EL register frame, a current
  task identity, a private address space, and a safe scheduler-stack handoff.
  Those mechanisms are not yet implemented.
- Until those prerequisites exist, the assembly path remains terminal.

## Implementation status

- Implemented and host-tested: origin/vector/ESR policy classification.
- Not implemented: EL0 vector entry frame, SVC dispatch, task termination on
  fault, or return-to-user recovery.
- Runtime behavior remains fail-stop for synchronous exceptions.
