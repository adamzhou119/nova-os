# ADR 0010: Per-Task Address Spaces and EL0 Isolation

- Status: Accepted for the single-core AArch64 prototype
- Date: 2026-10-31
- Scope: QEMU virt, 4 KiB granules, 39-bit TTBR0_EL1 translation

## Context

The current kernel builds one global EL1 stage-1 translation regime. Its
identity-mapped RAM and dynamically mapped virtual window are shared by every
task. Task control blocks and guarded kernel stacks exist, but tasks execute
with kernel privilege and do not own isolated user mappings.

Creating a user task on top of the current global page tables would not provide
process isolation: a task could address another task's mappings and kernel
memory. Address-space ownership must therefore be explicit before EL0 entry is
introduced.

## Decision

Each process/address space will own a distinct TTBR0_EL1 translation-table
root and a nonzero ASID. The current TCR_EL1 configuration leaves the ASID
size control clear, so ASIDs must be restricted to 8 bits (1..255). Enabling
16-bit ASIDs requires setting TCR_EL1.AS and adding architecture-level tests
before widening the allocator. The first implementation remains single-core.

### Mapping layout

- Preserve the current low-address QEMU platform map, including kernel RAM,
  page-table storage, GIC and PL011, as privileged-only mappings.
- Share immutable kernel/platform page-table subtrees between address spaces
  where the architecture permits it. Shared kernel mappings must never be
  writable from EL0.
- Allocate user virtual mappings from a separately defined user range. User
  mappings are represented in the owning address space only.
- Keep task kernel stacks in the privileged shared kernel map; they are never
  exposed to EL0.
- Use 4 KiB pages for initial user mappings. Large-page promotion is out of
  scope until page-table ownership and invalidation are proven.

### Permissions and ownership

- Add explicit user-accessible page permissions; do not infer EL0 access from
  the existing EL1 writable/executable flags.
- Enforce W^X per physical frame across aliases. Writable user pages are
  execute-never; executable user pages are not writable.
- Page-table pages are owned by the address-space object and remain pinned
  while reachable from a live TTBR0 root.
- Mapping, unmapping, and permission changes update the owning address space,
  perform the required break-before-make sequence where applicable, and
  invalidate translations for the affected ASID.
- An address space cannot be destroyed while a task is executing in it or
  while any mapped frame/page-table page remains referenced.

### Context switching and exceptions

- A task records an address-space reference in addition to its kernel stack
  and CPU context.
- Switching between tasks with different address spaces writes TTBR0_EL1 with
  the root and ASID, then performs the architecturally required barriers and
  ASID-scoped TLB invalidation.
- EL0 entry uses an exception frame that preserves x0-x30, ELR_EL1, SPSR_EL1,
  and the user stack pointer. IRQ preemption must save and restore this full
  frame; the current kernel-only IRQ frame is not sufficient by itself.
- SVC from EL0 is the system-call boundary. Synchronous user faults are
  attributed to the current task and terminate that task; kernel-originated
  faults remain terminal diagnostics.
- EL0 must not directly access GIC, timer, UART, page tables, or kernel stacks.

## Implementation order

1. Introduce address-space metadata, lifetime rules, and host tests without
   changing the active TTBR0.
2. Refactor page-table construction so a new root can share only privileged
   kernel mappings and own private user subtrees.
3. Add user mapping/unmapping and permission tests, including W^X alias cases.
4. Add ASID-aware address-space switching while all existing tasks still run
   in EL1.
5. Add a minimal EL0 trampoline, SVC syscall path, and user exception frame.
6. Prove isolation with two tasks mapping the same user VA to different frames
   and with negative cross-address-space access tests in QEMU.

## Explicit non-goals

This ADR does not enable EL0 execution, claim process isolation is implemented,
or enable SMP. It does not define a POSIX ABI, ELF loader, filesystem, or
multi-core TLB shootdown protocol. Those require separate decisions.
