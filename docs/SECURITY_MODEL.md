# Nova OS — Initial Security Model

Security is a foundational requirement, not a feature to bolt on after the AI layer.

## Core rules

1. **No implicit authority:** model-generated plans and tool arguments never grant permission.
2. **Least privilege:** each process, service, and tool adapter receives only the capabilities it needs.
3. **Independent enforcement:** the receiving service validates identity, arguments, and capabilities even if the caller claims a check already happened.
4. **Explicit confirmation:** require user confirmation for externally visible or difficult-to-reverse actions, including payments, purchases, sending messages, deleting data, and changing account/security settings.
5. **Data minimization:** expose only the minimum data needed for the task; redact secrets from logs and diagnostics.
6. **Cancellation and recovery:** users can cancel tasks, inspect completed actions, and recover from partial failure where possible.
7. **Auditable execution:** record the task, requested capabilities, confirmation decision, tool result, and outcome without recording unnecessary sensitive payloads.

## Proposed capability model

Capabilities should be explicit, scoped, revocable, and bound to a caller identity. A capability grant should describe:
- Subject: process or service identity.
- Resource: app, data object, or system service.
- Operations: read, write, execute, share, or other narrowly defined verbs.
- Scope: exact object or bounded set of objects.
- Lifetime: short-lived by default.
- Delegation: denied unless explicitly allowed by policy.

## AI orchestration boundary

The AI planner may propose a task plan. A deterministic policy layer must validate the plan, resolve required capabilities, and request user confirmation when required. The planner cannot modify policy, mint capabilities, or bypass service checks.

## Threats to address

- Prompt injection from app content, web pages, documents, and notifications.
- Confused-deputy behavior in tool adapters.
- Over-broad app and account permissions.
- Secret leakage through prompts, logs, crash reports, or telemetry.
- Replay and duplicate execution of non-idempotent actions.
- Partial completion when one step in a multi-step task fails.
- Malicious or compromised applications and system services.

## Status

This document defines design requirements. No capability enforcement framework or AI task executor has been implemented yet.
