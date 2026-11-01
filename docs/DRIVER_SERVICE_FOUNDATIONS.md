# Nova OS — Driver and System-Service Foundations

Status: the bounded kernel device-registry core and an initial FDT compatible/reg lookup
are implemented and host-tested, including indexed `reg` tuples and matching-node instance lookup. Nested-bus `ranges` address translation is implemented and host-tested. GICv2 interrupt decoding and interrupt-parent phandle lookup are implemented and host-tested. Single-device FDT-to-registry discovery is implemented and host-tested. The PL011 console now registers and probes through the registry after early boot; atomic enumeration and registration of every matching node are implemented and host-tested; additional hardware drivers remain in progress. The service manager, IPC transport, and capability enforcement are not implemented.

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

## Registry implementation status

The initial registry lives in `kernel/include/device.h` and `kernel/src/device.c`.
It stores a bounded set of discovered devices and compatible-string driver callbacks.
Probe transitions through `DISCOVERED → PROBING → READY` or `FAILED`; failed probes
release MMIO/IRQ claims, and removal releases claims only after the driver's remove
callback succeeds. Overlapping MMIO ranges and duplicate active IRQ ownership are
rejected. Host tests cover duplicate IDs/drivers, registry-capacity limits, invalid/overflowed MMIO ranges,
resource conflicts, failed-probe retry, and failed-removal retention.

The FDT helper in `kernel/src/fdt.c` can locate an exact compatible entry in a
NUL-separated `compatible` string list and decode an indexed `reg` tuple using the
parent bus's `#address-cells` and `#size-cells`. It applies ancestor bus `ranges` translations, including multi-level buses, and rejects a nested bus with a missing or non-matching translation. `nova_fdt_find_gicv2_irq` resolves inherited/overridden `interrupt-parent` phandles and the first three-cell GICv2 `interrupts` tuple. `interrupts-extended` and non-GICv2 controllers are not yet supported. `nova_device_discover_fdt` combines translated MMIO and optional GICv2 IRQ discovery into a registry descriptor. `nova_fdt_find_device_instance` and `nova_fdt_find_gicv2_irq_instance` distinguish multiple nodes sharing one compatible string, and `nova_device_discover_fdt_instance` registers a selected node instance with its own MMIO and optional IRQ. `nova_device_discover_fdt_all` now preflights and atomically registers every matching node's first `reg` tuple, assigns consecutive caller-provided IDs, and leaves the registry unchanged on malformed input, ID collision/overflow, or capacity failure. Host tests cover two-node enumeration, per-instance IRQ/MMIO values, collision rejection, and ID overflow. The PL011 console is the first concrete driver: it discovers `arm,pl011`, registers its probe/remove callbacks, and switches its MMIO base after a successful probe. GICv2 initialization attempts to resolve distributor and CPU-interface windows from the first two `reg` tuples; it retains the QEMU virt fixed-address fallback if discovery is unavailable or invalid. The timer PPI remains platform-configured.

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
| GICv2 | Distributor and CPU-interface MMIO windows resolved from FDT; timer PPI 30 remains configured | Add controller registry lifecycle and validate interrupt-controller variants |
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
