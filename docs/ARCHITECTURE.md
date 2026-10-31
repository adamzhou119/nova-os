# Nova OS — Initial Architecture

## Layers
1. **Boot/platform:** firmware handoff, CPU setup, memory map, timers, interrupts, UART, device discovery.
2. **Kernel:** address spaces, scheduling, synchronization, IPC primitives, capability enforcement.
3. **System services:** storage, networking, display, input, audio, power, identity, notifications, app lifecycle.
4. **Application runtime:** packaging, sandboxing, permissions, lifecycle, user applications.
5. **AI orchestration:** intent interpretation, task planning, tool selection, permission checks, execution monitoring, recovery.
6. **Shell:** natural-language entry point alongside conventional visual controls and accessibility.

## AI task lifecycle
`Received → Clarifying → Planned → Permission Check → Awaiting Confirmation → Executing → Verifying → Completed / Failed / Cancelled`

Not every task needs clarification or confirmation. The model is a planner, never an authorization authority. Tool adapters validate arguments and enforce capabilities. Consequential actions require confirmation under policy. Users can cancel and inspect outcomes.

## Trust boundaries
- Treat model output as untrusted input.
- Enforce permissions in system services, not only in prompts.
- Isolate app data by default.
- Keep secrets out of prompts and logs unless strictly required and protected.
- Audit actions and redact sensitive values.

## Kernel prototype boundary
The `kernel/` directory is an experimental AArch64 bare-metal starter for QEMU `virt`. It aims to establish an entry point and serial output only. It does not implement memory management, interrupts, scheduling, drivers, or user-mode processes.

## Assumptions to validate
The starter uses the conventional QEMU virt PL011 UART address. Boot exception level, device-tree handling, and exact QEMU invocation must be verified at runtime.

## Open decisions
- From-scratch kernel versus an existing open-source kernel.
- Reference device and boot firmware.
- Kernel/system-service language choices.
- On-device versus remote inference split.
- Permission, confirmation, and audit data model.
- Compatibility requirements for existing mobile apps.
