# Nova OS

An AI-native mobile operating system built from scratch.

Nova OS explores a mobile operating system in which AI is a first-class interaction layer. The long-term goal is to let users express intent in natural language and coordinate tasks across applications with clear permissions, predictable behavior, and user control.

## Project status

**Stage:** Milestone 0 — project foundation and architecture.

This repository contains planning documents and an experimental AArch64 bare-metal kernel starter targeting QEMU's `virt` machine. The kernel is a bring-up scaffold, not a usable mobile OS. A complete QEMU runtime boot has not yet been verified.

## Start here

- [Project charter](docs/PROJECT_CHARTER.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Kernel prototype](kernel/README.md)

## Principles

1. AI-first interaction with explicit user control.
2. Least-privilege security and privacy by design.
3. Small, testable milestones and reproducible builds.
4. Honest status reporting: designed, implemented, build-verified, runtime-verified, and hardware-verified are distinct states.

## Current limitations

No phone hardware drivers, application runtime, networking stack, graphical shell, or on-device AI runtime are implemented yet.

## License

No license has been selected. All rights are reserved by the repository owner.
