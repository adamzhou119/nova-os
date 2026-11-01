# Nova OS — Initial Architecture

## Architectural layers

1. **Boot and platform layer** — firmware handoff, CPU setup, memory map, timers, interrupt controller, serial console, and device discovery.
2. **Kernel layer** — address spaces, scheduling, synchronization, IPC primitives, and capability enforcement.
3. **System services** — storage, networking, display, input, audio, power, identity, notifications, and application lifecycle.
4. **Application runtime** — packaging, sandboxing, permissions, lifecycle, and user-facing applications.
5. **AI orchestration layer** — intent interpretation, task planning, tool selection, execution monitoring, permission checks, and recovery.
6. **Shell and interaction layer** — natural-language entry point plus conventional visual controls and accessibility.

## AI task execution model

A task should move through explicit states:

`Received → Clarifying (if needed) → Planned → Permission Check → Awaiting Confirmation (if needed) → Executing → Verifying → Completed / Failed / Cancelled`

Model output is not authorization. Each tool call must be checked against a capability policy. Consequential actions require confirmation according to policy. Users must be able to cancel and inspect what happened.

## Trust boundaries

- The language model is an untrusted planner, not a security authority.
- Tool adapters validate arguments and enforce capabilities.
- System services authenticate callers and check permissions independently.
- App data is isolated by default.
- Secrets are not placed in prompts or logs unless explicitly required and protected.
- Actions should be auditable, with sensitive values redacted.

## Kernel prototype boundary

The `kernel/` directory is an experimental bare-metal AArch64 kernel for QEMU `virt`. It now includes FDT-backed physical memory allocation, a bounded kernel virtual mapping window with W^X checks, EL1 exception diagnostics, a GICv2/generic-timer path, task lifecycle management, cooperative switching, and single-core timer-driven IRQ preemption. It remains single-core. It has EL0 tasks and private address spaces, but does not yet provide SMP, a general driver model, networking/storage, or the system-service and application layers.

## Platform assumptions to validate

The prototype targets QEMU's AArch64 `virt` machine and uses the PL011 UART at the conventional QEMU virt physical address. Boot exception level, device-tree handling, and QEMU command line must be verified by runtime tests before being treated as stable interfaces.

## Key open decisions

- Whether to build on an existing open-source kernel or continue from scratch.
- Which reference hardware and boot firmware to target.
- Which language mix to use for kernel and system services.
- How model inference is split between on-device and remote execution.
- How permissions, user confirmation, and audit records are represented.
- What compatibility layer, if any, is required for existing mobile applications.
