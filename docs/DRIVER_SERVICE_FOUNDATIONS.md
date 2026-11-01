# Nova OS — Driver and System-Service Foundations

Status: interface design only. This document does not claim that the service manager,
IPC transport, capability enforcement, or device-driver framework is implemented.

## Scope and order

The first reference platform remains the single-core QEMU AArch64 `virt` machine.
The implementation order is deliberately narrow:

1. Keep early boot dependencies limited to UART, timer, interrupt controller,
   FDT, and memory initialization.
2. Move hardware access behind small driver interfaces before adding more devices.
3. Introduce a kernel-owned device registry with explicit probe/ready/failed states.
4. Add a bounded IPC mechanism and service lifecycle only after the kernel has
   stable task scheduling, address-space isolation, and fault containment.
5. Put AI orchestration above services; it must never bypass service permission
   checks or call device registers directly.

## Driver contract

A future driver instance should expose:
- a stable device identifier and compatible string sourced from the FDT;
- a lifecycle: `DISCOVERED → PROBING → READY | FAILED`;
- resource ownership for MMIO ranges, IRQ lines, and DMA buffers;
- explicit initialization and shutdown hooks;
- bounded request handling with defined error codes;
- host-testable logic separated from architecture-specific MMIO access.

A driver must not claim an MMIO range or interrupt already owned by another
instance. Failed probe must release every resource acquired during that probe.
Device-tree values are untrusted input: validate address/size arithmetic,
interrupt numbers, and supported compatible strings before touching hardware.

## Service contract

A service is a task with:
- a stable service ID and versioned request/response ABI;
- a declared capability set;
- a lifecycle: `CREATED → STARTING → READY → STOPPING → STOPPED`, with
  `FAILED` reachable from any active state;
- bounded message sizes and explicit caller identity;
- cancellation, timeout, and crash reporting semantics.

The kernel validates endpoint identity and memory ownership. A service validates
its own request schema and capability requirements. A model-generated plan is
never authorization. Consequential operations require policy approval and, where
specified, explicit user confirmation.

## Initial device boundaries

| Device | Current state | Next safe extraction |
|---|---|---|
| PL011 UART | QEMU-specific MMIO output in kernel code | Console API plus isolated PL011 backend |
| ARM generic timer | Implemented for QEMU virt | Keep timer core independent of register access |
| GICv2 | Implemented for QEMU virt | FDT-described interrupt-controller registration |
| Block storage | Not implemented | Read-only virtio block prototype after DMA/IOMMU policy |
| Display/input/network | Not implemented | Defer until a documented reference board exists |

## Required tests before calling this a framework

- Duplicate MMIO and IRQ ownership is rejected.
- Probe failure unwinds all acquired resources.
- Unsupported FDT compatible strings are ignored safely.
- Service messages reject oversized, malformed, and unauthorized requests.
- A crashed service can be detected and restarted without corrupting peer state.
- Every privileged operation is denied by default unless a capability is present.

## Explicit non-goals

This milestone does not promise Android compatibility, phone hardware support,
SMP, DMA isolation, a production IPC transport, or an operational AI agent.
Those require separate designs and hardware-backed validation.
