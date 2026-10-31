# Nova OS — Project Charter

## Vision
Nova OS is an AI-native mobile operating system. Users express goals in natural language and the system helps complete tasks across applications while preserving user agency, privacy, and security.

## Product definition
> Nova OS is a mobile operating system centered on AI interaction, enabling users to complete cross-application tasks through natural language with fewer manual steps.

This is the product direction, not a claim that the capability already exists.

## Principles
- **Intent before interface:** start from the user's goal.
- **User control:** request confirmation before purchases, payments, external messages, deletion, and other consequential actions.
- **Least privilege:** grant each app and service only the access required.
- **Transparency:** expose plans, progress, data use, failures, and recovery options.
- **Reliability:** optimize for successful, recoverable task completion.
- **Modularity:** separate kernel, system services, AI orchestration, and UI.

## Initial scenarios to validate
1. Find a calendar opening and prepare a message proposing it.
2. Collect information from multiple apps and summarize it.
3. Turn a conversation into a reminder after user confirmation.
4. Execute a multi-step workflow while showing the planned actions.

These are research scenarios, not implemented features.

## Success measurement
Primary metric: **AI task completion success rate** = tasks meeting their predefined acceptance criteria / eligible tasks attempted.

Track clarification, cancellation, failure, user correction, time to completion, recovery success, and unintended/unauthorized actions separately. Unauthorized side effects must be treated as critical failures. No numeric target is approved yet; establish a baseline before setting one.

## First-milestone scope
Establish repository structure, architecture notes, and a reproducible virtual bring-up path for an experimental AArch64 kernel.

A complete mobile OS additionally requires boot integration, memory and process management, drivers, graphics, input, power management, security, application runtime, update/recovery, and hardware validation.

## Non-goals for Milestone 0
- Commercial smartphone support.
- Android/iOS application compatibility.
- A consumer-ready AI assistant.
- Claims of arbitrary phone hardware compatibility.
- An unvalidated task-success percentage.

## Decision discipline
Record material product and architecture decisions. Prefer small isolated changes, repeatable tests, and explicit labels for provisional decisions.
