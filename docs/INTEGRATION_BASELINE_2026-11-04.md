# Nova OS Integration Baseline — 2026-11-04

This document records the verified GitHub integration state. It is a coordination artifact, not a claim that the listed pull requests have been merged or that the OS is complete.

## Repository visibility

- Default branch: `main` (observed head: `b96096b67f54b313dfdb6876749a48123efec32c`).
- Latest capability-broker implementation branch: `feat/kernel-capability-broker-a2` (head: `7d22b1694b86398512989d5b5f7f6a46a2ec1b16`).
- PR #25 is open, Draft, and unmerged. Its CI run #41816225905 completed successfully with 17/17 steps.
- Feature branches and Draft PRs are intentionally not visible in the `main` tree until integrated. A repository homepage view of `main` is therefore not a complete progress view.

## Verified recent work

| PR | Branch | Base | Scope | State |
|---|---|---|---|---|
| #19 | `feat/user-file-descriptor-syscalls` | `feat/vfs-ramfs-mount` | EL0 file descriptor open/close/read/write | Open, Draft, unmerged |
| #20 | `feat/kernel-ipc-mailboxes` | `feat/ai-plan-executor` | Bounded IPC ports and message queues | Open, Draft, unmerged |
| #21 | `feat/kernel-service-registry` | `feat/kernel-ipc-mailboxes` | Owner-bound service discovery | Open, Draft, unmerged |
| #22 | `feat/el0-service-discovery-syscalls` | `feat/kernel-service-registry` | EL0 service and port lifecycle calls | Open, Draft, unmerged |
| #23 | `feat/el0-ipc-message-syscalls` | `feat/el0-service-discovery-syscalls` | EL0 IPC send/receive and port rights | Open, Draft, unmerged |
| #24 | `feat/kernel-capability-broker` | `feat/el0-ipc-message-syscalls` | First capability-registry implementation | Open, Draft, unmerged |
| #25 | `feat/kernel-capability-broker-a2` | `feat/el0-ipc-message-syscalls` | A2 capability broker refinement and regression coverage | Open, Draft, unmerged |

## Integration blockers

1. **Two parallel file/IPC histories:** PR #19 is based on the VFS/RAMFS chain, while PR #20–#25 form a separate IPC/capability chain. Neither chain is integrated into `main`.
2. **Syscall ABI collision:** PR #19 assigns syscall IDs 3–6 to file descriptor operations. PR #22 uses IDs 3–7 for IPC-port/service lifecycle operations; PR #23 continues with IDs 8–11. These cannot be combined as-is.
3. **Duplicate capability work:** PR #24 and #25 are parallel implementations based on PR #23. Treat #25/A2 as the current candidate for review; do not merge both blindly.
4. **Broker bypass remains:** EL0 IPC grant/revoke calls still modify the underlying IPC rights table outside the capability broker. The broker is a kernel foundation, not yet the sole authorization path.
5. **No merge authorization:** All PRs listed above remain Draft/unmerged. This project record does not authorize merging.

## Required integration order

1. Select one canonical capability-broker implementation after reviewing #24 versus #25; retain the other as historical work until its unique changes are reconciled.
2. Define one central, versioned syscall ABI covering task, file, IPC, and service operations. Update kernel dispatch, user stubs, QEMU EL0 tests, and documentation together.
3. Rebase or stack the file-descriptor work onto the chosen integration chain and resolve syscall numbering without silently changing existing semantics.
4. Route privileged EL0 IPC rights changes through broker policy, or remove the raw EL0 grant/revoke surface until a trusted broker protocol exists.
5. Run the complete host and QEMU CI suite on the combined candidate before requesting review or merge.
6. Only after the combined candidate is verified should maintainers decide whether to merge it into `main`.

## Product-level milestones (honest status)

- Kernel boot, exception handling, memory management, task/scheduler foundations: early implementation exists; continued hardening required.
- VFS/RAMFS and EL0 file I/O: implementation exists on a separate branch/PR chain; not integrated.
- IPC and service discovery: bounded kernel and EL0 foundations exist on a separate PR chain; blocking waits, cancellation, deadlines, and SMP synchronization remain.
- Capability broker: IPC-only kernel authorization foundation exists; consent, policy schemas, complete EL0 mediation, app/device resource types, and user-visible audit remain.
- AI runtime and app integration: incomplete; typed plan validation/execution is only a foundation.
- GUI, launcher, mobile hardware drivers, and real-device runtime: not yet delivered.
- iOS-level performance or superiority: an aspiration only; requires a usable device implementation and reproducible hardware benchmarks.

## Update rule

For every future progress report, distinguish:
- committed code and its exact branch/commit;
- CI actually run and its exact result;
- open Draft PRs versus merged code on `main`;
- implemented behavior versus roadmap/documentation.
Do not count a Draft PR as integrated into the product baseline.
