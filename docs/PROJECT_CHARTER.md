# Nova OS — Project Charter

## Vision

Nova OS is an AI-native mobile operating system. Users express goals in natural language and the system helps complete tasks across applications while preserving user agency, privacy, and security.

## Product definition

> Nova OS is a mobile operating system centered on AI interaction, enabling users to complete cross-application tasks through natural language with fewer manual steps.

This is the product direction, not a claim that these capabilities already exist.

## Product principles

- **Intent before interface:** start from what the user wants to accomplish, not from a fixed sequence of screens.
- **User remains in control:** explain planned actions and request confirmation before purchases, payments, messages to others, deletion, or other consequential actions.
- **Permission boundaries:** apps and system services receive only the access needed for a task.
- **Transparency:** make task progress, data use, failures, and recovery options understandable.
- **Reliability over spectacle:** optimize for successful, recoverable task completion rather than novelty.
- **Modular foundations:** keep the kernel, system services, AI orchestration, and user interface separable.

## Initial target scenarios

Candidate scenarios for later validation:
1. Find a time in the calendar and prepare a message proposing it.
2. Collect information from several apps and produce a concise summary.
3. Create a reminder from a conversation, with the user confirming the details.
4. Complete a multi-step workflow while showing the user what will happen.

These are research scenarios, not implemented features.

## Success measurement

The primary product metric is **AI task completion success rate**:

Successful tasks / eligible tasks attempted.

A task counts as successful only when its stated acceptance criteria are met and the user can inspect the result. Failures, cancellations, and clarification requests must be tracked separately.

Supporting metrics:
- User-confirmed correction rate.
- Median time to completion.
- Unintended or unauthorized action rate (target: zero).
- Recovery success rate after a tool or app failure.
- User-reported trust and clarity.

No numeric target is approved yet. Establish a baseline with a defined test set before setting a target.

## Scope boundaries

The project is starting from an experimental AArch64 bare-metal learning prototype. A complete mobile OS requires boot integration, kernel services, memory and process management, drivers, graphics, input, power management, security, application runtime, update/recovery, and hardware validation.

The first milestone is repository foundation and a reproducible virtual bring-up path. It is not a promise of immediate phone support.

## Non-goals for the first milestone

- Supporting a commercial smartphone.
- Running Android or iOS applications.
- Shipping a consumer-ready AI assistant.
- Claiming compatibility with arbitrary phone hardware.
- Setting an unvalidated task-success percentage.

## Decision process

Record material architecture and product decisions in the repository. Prefer small, testable milestones, reproducible builds, and explicit notes when a decision is provisional.
