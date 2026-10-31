# Nova OS

An AI-native mobile operating system built from scratch.

Nova OS is an early-stage research and engineering project exploring a mobile operating system in which AI is a first-class interaction layer. The long-term goal is to let users express intent in natural language and have the system coordinate tasks across applications with clear permissions, predictable behavior, and user control.

## Project status

**Stage:** Early kernel bring-up (AArch64 QEMU prototype).

The repository contains project planning documents and an experimental bare-metal kernel targeting QEMU's `virt` machine. The kernel has been compiled and its serial boot path has passed a GitHub Actions QEMU smoke test. Early `.bss` initialization is now checked by a runtime probe in CI. This remains a learning and engineering scaffold, not a usable mobile operating system.

## Start here

- [Project charter](docs/PROJECT_CHARTER.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture overview](docs/ARCHITECTURE.md)
- [Early boot contract](docs/BOOT_PROTOCOL.md)
- [Build and test guide](docs/BUILD.md)
- [Kernel prototype notes](kernel/README.md)

## Guiding principles

1. AI-first interaction, with explicit user control and confirmation for consequential actions.
2. Security and privacy by design; least privilege is the default.
3. Small, testable milestones with reproducible builds.
4. Honest status reporting: distinguish implemented, compiled, boot-tested, and production-ready.

## Current limitations

Nova OS is not based on Android or Linux yet. It has no phone hardware drivers, application runtime, networking stack, graphical shell, or on-device AI runtime. These are future research areas, not completed features.

## License

No license has been selected yet. Until one is added, all rights are reserved by the repository owner.
