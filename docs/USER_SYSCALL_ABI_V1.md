# Nova OS EL0 Syscall ABI v1

The EL0 AArch64 syscall convention is shared by every user-facing subsystem:
- x8: syscall number
- x0-x5: arguments
- x0: return value
- negative values: negative errno-style errors
- SVC instruction immediate: 0

All syscall numbers are unique in this table. This table is the canonical source;
kernel dispatch, user stubs, assembly test payloads and future SDK headers must
use these definitions rather than private numeric literals.

| Number | Symbol | Operation | Arguments |
|---:|---|---|---|
| 0 | `NOVA_SYS_TASK_ID` | Get current task ID | — |
| 1 | `NOVA_SYS_EXIT` | Exit current task | x0=exit code |
| 2 | `NOVA_SYS_CONSOLE_WRITE` | Write to kernel console | x0=user buffer, x1=length |
| 3 | `NOVA_SYS_FILE_OPEN` | Open/create RAMFS file | x0=user path, x1=flags |
| 4 | `NOVA_SYS_FILE_CLOSE` | Close task-owned descriptor | x0=descriptor |
| 5 | `NOVA_SYS_FILE_READ` | Read from descriptor | x0=descriptor, x1=user buffer, x2=length |
| 6 | `NOVA_SYS_FILE_WRITE` | Write to descriptor | x0=descriptor, x1=user buffer, x2=length |
| 7 | `NOVA_SYS_IPC_PORT_CREATE` | Create task-owned IPC port | — |
| 8 | `NOVA_SYS_SERVICE_REGISTER` | Register service name | x0=user name, x1=owned port |
| 9 | `NOVA_SYS_SERVICE_LOOKUP` | Look up service | x0=user name |
| 10 | `NOVA_SYS_SERVICE_REMOVE` | Remove owned service | x0=user name |
| 11 | `NOVA_SYS_IPC_PORT_CLOSE` | Close owned IPC port | x0=port ID |
| 12 | `NOVA_SYS_IPC_PORT_GRANT` | Grant port rights | x0=port, x1=recipient task, x2=rights |
| 13 | `NOVA_SYS_IPC_PORT_REVOKE` | Revoke port rights | x0=port, x1=recipient task |
| 14 | `NOVA_SYS_IPC_SEND` | Send bounded message | x0=port, x1=user payload, x2=length |
| 15 | `NOVA_SYS_IPC_RECEIVE` | Receive bounded message | x0=port, x1=user message, x2=capacity |

## Compatibility and implementation status

This is a coordinated ABI change from the earlier parallel Draft branches.
The file descriptor implementation originally used IDs 3-6; the IPC branch
originally used IDs 3-11. Those private ranges collided. ABI v1 reserves 3-6
for file descriptors and relocates IPC/service calls to 7-15.

The canonical header is `kernel/include/syscall.h`. At this integration stage,
the capability/IPC kernel dispatch and EL0 IPC regression payload have been
updated to the new IPC numbers. The file descriptor implementation still needs
to be ported from PR #19 into this integration branch, and its dispatch and
end-to-end tests must then be wired to IDs 3-6 before this ABI is considered
fully integrated or validated.

This remains a Nova-specific early ABI, not POSIX compatibility. The mounted
filesystem on the file branch is volatile flat /ramfs; seek, persistence,
directories, permission model and SMP-safe filesystem locking are not provided.
