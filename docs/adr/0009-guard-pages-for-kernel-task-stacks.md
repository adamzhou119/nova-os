# ADR 0009: Guard Pages for Kernel Task Stacks

- Status: Accepted
- Date: 2026-10-31
- Scope: Single-core AArch64 kernel stacks in the QEMU virt virtual window

## Context

Each task previously received one mapped 4 KiB kernel stack page. Because task stacks grow downward, an overflow could cross the stack's lower boundary and corrupt an adjacent virtual allocation without immediately faulting.

The kernel virtual allocator uses a bitmap to track occupied virtual pages. A guard page must remain unavailable to ordinary allocations even though it has no physical mapping.

## Decision

Every task stack is allocated as a pair of adjacent virtual slots:

- The lower slot is reserved in the virtual allocator bitmap but remains unmapped.
- The upper slot is backed by a physical page and is the task's usable 4 KiB stack.

The guard is immediately below the usable stack so a downward-growing stack faults on its first page overflow.

The allocator exposes paired APIs:

- `nova_vmap_alloc_guarded_page` finds two adjacent free virtual slots, allocates and maps the upper slot, then reserves both bitmap slots.
- `nova_vmap_free_guarded_page` validates the reserved-but-unmapped guard and mapped stack page, unmaps the stack, releases both virtual slots, and frees its physical frame.
- `nova_vmap_free_page` refuses to free the mapped half of a guarded allocation as an ordinary page, preventing a leaked guard reservation.

Task creation uses the guarded allocator. Task destruction releases the stack and guard together, only after the task has been reaped on the scheduler stack.

## Validation

Automated host tests verify:
- The guard slot is not translated.
- Ordinary page allocation skips both reserved slots.
- A guarded stack cannot be freed through the ordinary single-page API.
- Freeing a guarded stack releases both slots for later reuse.
- Duplicate, unaligned, and mismatched frees are rejected.

QEMU tests verify:
- Each created task has an unmapped page immediately below its stack.
- Reading the guard page raises an EL1 data abort with FAR_EL1 equal to the guard virtual address.

Latest full CI success: workflow run 41198720520, commit 2866c7a.

## Consequences and limits

This provides a one-page guard below each kernel stack. It does not detect overflow that skips over the guard page, provide a per-task stack high-water mark, or replace stack-size analysis. The allocator and guard metadata remain single-core and are not protected for SMP concurrency.
