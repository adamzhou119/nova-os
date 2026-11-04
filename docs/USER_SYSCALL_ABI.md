# Nova OS user syscall ABI (initial)

The current EL0 SVC ABI uses register x8 for the syscall number and returns
the result in x0. Arguments are passed in x0 through x2. Negative values are
negative errno-style results.

| Number | Operation | Arguments | Result |
|---:|---|---|---|
| 0 | get task ID | none | task ID |
| 1 | exit | x0=exit_code | does not return |
| 2 | console write | x0=user_buffer, x1=length | bytes written |
| 3 | open | x0=user_path, x1=flags | descriptor |
| 4 | close | x0=descriptor | 0 |
| 5 | read | x0=descriptor, x1=user_buffer, x2=length | bytes read |
| 6 | write | x0=descriptor, x1=user_buffer, x2=length | bytes written |

## Current limits

- The only mounted filesystem is volatile, flat /ramfs.
- open accepts only NOVA_FILE_OPEN_CREATE (bit 0); it does not truncate.
- Descriptors are task-owned and are closed automatically on task exit or
  contained EL0 fault. A descriptor cannot be used by another task.
- Read/write are limited to 256 bytes per call. File capacity is 4096 bytes.
- Open-file offsets are sequential. There is no seek, append mode, directory
  descriptor, permissions model, persistence, or SMP locking yet.
- The implementation currently assumes the existing single-core kernel.
- This is an initial Nova-specific ABI, not POSIX compatibility.

The kernel copies user paths and buffers into bounded kernel storage and checks
user address permissions before accessing them.
