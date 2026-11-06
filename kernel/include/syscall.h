#ifndef NOVA_SYSCALL_H
#define NOVA_SYSCALL_H

/*
 * Nova OS EL0 syscall ABI v1.
 * x8 selects the operation; x0-x5 carry arguments; x0 returns the result.
 * Numbers are globally unique across task, console, file, IPC and service APIs.
 */
#define NOVA_SYS_TASK_ID          0U
#define NOVA_SYS_EXIT             1U
#define NOVA_SYS_CONSOLE_WRITE    2U

/* Task-owned file descriptors. */
#define NOVA_SYS_FILE_OPEN        3U
#define NOVA_SYS_FILE_CLOSE       4U
#define NOVA_SYS_FILE_READ        5U
#define NOVA_SYS_FILE_WRITE       6U

/* IPC ports and named service discovery. */
#define NOVA_SYS_IPC_PORT_CREATE  7U
#define NOVA_SYS_SERVICE_REGISTER 8U
#define NOVA_SYS_SERVICE_LOOKUP   9U
#define NOVA_SYS_SERVICE_REMOVE  10U
#define NOVA_SYS_IPC_PORT_CLOSE  11U
#define NOVA_SYS_IPC_PORT_GRANT  12U
#define NOVA_SYS_IPC_PORT_REVOKE 13U
#define NOVA_SYS_IPC_SEND        14U
#define NOVA_SYS_IPC_RECEIVE     15U

#endif
