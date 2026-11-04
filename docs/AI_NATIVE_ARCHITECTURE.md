# Nova OS AI-Native System Architecture

Status: architecture baseline, not an implemented AI runtime.

## Product contract

Nova OS treats intelligence as a system capability. The user expresses an outcome; the system can discover permitted capabilities, build a bounded plan, execute approved steps, report progress, and recover or cancel. The model is replaceable and is never trusted as a privileged component.

The two product-level acceptance goals remain:
1. Responsiveness: interaction latency, frame pacing, launch time, task-switch latency, and energy use must be measured on real target hardware before any comparison with iOS.
2. Useful intelligence: end-to-end task completion, permission correctness, cancellation, recoverability, and privacy must be measured—not just model benchmark scores.

## Trust boundaries

- **Kernel:** scheduling, address spaces, memory protection, IPC primitives, timers, device access control. No model inference and no natural-language parsing in kernel mode.
- **System capability broker:** owns capability discovery, policy checks, user consent, rate limits, audit events, and revocation. A model cannot grant itself permissions.
- **AI orchestration service (unprivileged):** receives user intent, obtains capability descriptions, produces a typed plan, requests broker authorization, executes one step at a time, and handles cancellation/recovery.
- **Model runtime:** untrusted planner. It proposes structured plans only; it cannot issue raw syscalls, access arbitrary memory, or bypass the broker.
- **Applications and drivers:** expose narrow, versioned capability interfaces; they do not receive unrestricted access to another app's private data.

## Request lifecycle

1. Capture intent and identify the requesting user/session.
2. Resolve ambiguity; ask the user when the target or consequence is unclear.
3. Discover capabilities and their schemas from the broker.
4. Produce a typed plan with bounded steps, explicit inputs/outputs, and declared side effects.
5. Validate every step against capability policy, app identity, data scope, and resource budgets.
6. Obtain explicit consent for sensitive or externally visible actions.
7. Execute sequentially or as a dependency graph; checkpoint progress and emit user-visible status.
8. On failure, stop dependent steps, run only declared compensations, and preserve an audit trail.
9. Allow cancellation and permission revocation at every safe boundary.

## Capability contract

Every capability must publish:
- stable capability ID and schema version;
- owner service and caller eligibility;
- input/output schema and maximum payload size;
- read/write/network/device side-effect classification;
- data scope and retention behavior;
- whether explicit user confirmation is mandatory;
- timeout, cancellation, idempotency, and compensation behavior.

Capabilities must be deny-by-default. Unknown versions, unknown fields, oversized payloads, stale consent, and missing ownership checks are rejected.

## Scheduling and responsiveness

AI work must not starve touch, display, audio, or system-critical tasks. The orchestration service should use:
- bounded CPU and memory budgets per request;
- cancellable work units and deadlines;
- priority classes with starvation protection;
- asynchronous IPC rather than blocking the UI thread;
- back-pressure for long-running model or tool calls.

The kernel scheduler remains policy-neutral. Product-level priority decisions belong to a system service with auditable rules.

## Privacy and safety requirements

- Local processing is preferred when capability and quality permit.
- Network/model-provider access is explicit, scoped, and visible.
- Private app data is passed only through broker-approved, purpose-limited handles.
- Secrets and authentication tokens are never inserted into model prompts.
- Destructive, financial, identity, communication, and external-publication actions require explicit confirmation.
- Every request supports cancellation; revocation blocks future steps.
- Audit records describe capability IDs and outcomes without copying sensitive payloads.

## Reliability and recovery

Plans are durable state machines, not opaque model transcripts. Each step records a stable operation ID, input digest, start/result state, and idempotency key where supported. Retries are permitted only for idempotent operations or after an explicit reconciliation step. Compensation is best-effort and must never be represented as a guaranteed rollback.

## Required acceptance tests

1. Model-proposed unknown capability is rejected.
2. Cross-application data access without a scoped grant is rejected.
3. Sensitive action cannot execute without fresh user approval.
4. Revocation and cancellation stop subsequent plan steps.
5. A timed-out or crashed tool does not block touch/UI scheduling.
6. Duplicate delivery does not duplicate a non-idempotent operation.
7. Partial failure is surfaced with completed and pending steps clearly distinguished.
8. CPU/memory budget exhaustion terminates or yields the AI task without starving system-critical work.
9. Offline mode remains usable for core OS functions.
10. Performance is measured on target hardware using touch-to-feedback, frame-time percentiles, app launch, task-switch, and energy metrics.

## Implementation sequence

- **A0 — Contract:** this document and versioned capability schema.
- **A1 — IPC:** bounded kernel mailbox transport and kernel-local service discovery are implemented as initial primitives. Mailboxes support owner-controlled send/receive grants, revocation, and task teardown. The registry supports owner-bound registration, lookup, duplicate rejection, unregister, and automatic removal on task exit. Service lookup returns an endpoint ID but does not grant access; IPC grants remain separately enforced. EL0 can now create and close its own IPC port and register, look up, or remove its own named service through SVC calls. Service names are copied byte-by-byte into a bounded kernel buffer, stop at NUL, and reject inaccessible pointers, empty/oversized names, invalid characters, and unauthorized ownership. EL0 can now send and receive bounded messages through SVC, and port owners can grant or revoke SEND/RECEIVE rights for an existing task. Send copies at most 64 bytes into kernel memory; receive preflights the complete writable user result before dequeuing, so an invalid output pointer does not consume a queued message. Calls are non-blocking and return an EAGAIN-style error for an empty/full queue. Blocking waits, deadlines, cancellation tokens, capability-broker integration, consent/audit policy, and SMP synchronization remain incomplete.
- **A2 — Broker:** capability registry, permission policy, consent UI hooks, and audit. The kernel-local registry is bounded to 32 entries and deny-by-default; only the IPC-port owner can issue grants, and each grant binds holder, resource, and rights. Grant/revoke now update the existing IPC enforcement table transactionally, authorization rechecks effective IPC rights and resource ownership, handles are monotonic/non-reused, and task teardown clears records for both issuers and holders. Host tests cover default denial, unauthorized issuer, nonexistent holder, unsupported resource type, invalid rights, least-privilege checks, duplicate-grant rejection, underlying IPC grant/revoke, out-of-band rights revocation, stale handles, and task cleanup. This remains a kernel policy primitive rather than the complete system broker: existing EL0 IPC grant/revoke SVC calls can still modify the IPC grant table without going through the broker; consent UI, audit trail, capability discovery, EL0 broker protocol, expiry, and policy persistence remain unimplemented.
- **A3 — Orchestrator:** typed plan validation, dependency execution, checkpoints, and recovery.
- **A4 — Model adapter:** replaceable local/remote model providers behind the planner interface.
- **A5 — Product integration:** launcher, settings, files, communications, and cross-app workflows.
- **A6 — Evaluation:** privacy, safety, reliability, responsiveness, and energy test suites.

No phase is considered complete from documentation alone. Each requires implementation and automated tests. The kernel now also has a bounded service-name registry (16 entries, names up to 31 ASCII characters). Registration is accepted only when the caller owns the referenced IPC port; duplicate names are rejected, and entries are removed when their owner exits. Lookup returns an endpoint ID but does not grant send/receive rights, so IPC authorization remains a separate explicit operation. The registry is exposed to EL0 through a narrow SVC ABI, but it is not yet connected to the capability broker or consent/audit policy.

The current mailbox and registry primitives plus EL0 port/service discovery, message transfer, and rights-management calls are still an early A1 service IPC foundation; blocking delivery, capability mediation, consent/audit, deadlines, cancellation, and SMP-safe synchronization are not yet implemented.
