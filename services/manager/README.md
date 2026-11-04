# User service manager foundation

This directory provides a portable lifecycle state machine for the future
user-space service manager. It tracks service identity, owning task, published
IPC endpoint, and startup, running, shutdown, and failure transitions.

The manager does not create kernel tasks or mint capabilities. Those actions
must be supplied by a privileged launch adapter after Nova OS exposes a
constrained task-launch and capability-delegation interface. A service may
publish an endpoint only for the task identity assigned to its current start
attempt. Stale task identities cannot publish or stop a newer instance.

The AI plan and executor modules remain policy-neutral building blocks. A
future AI service can be supervised through this lifecycle API while each
operation continues to require independent capability authorization.

Not implemented yet: dependency graphs, restart policy, persistent manifests,
an EL0 manager process, and the kernel launch adapter.
