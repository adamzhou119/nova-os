#ifndef NOVA_SYSCALL_H
#define NOVA_SYSCALL_H

/*
 * Stable EL0 AArch64 syscall numbers.
 * x8 selects the operation; x0-x5 carry arguments; x0 returns the result.
 */
#define NOVA_SYS_TASK_ID          0U
#define NOVA_SYS_EXIT             1U
#define NOVA_SYS_WRITE            2U
#define NOVA_SYS_IPC_PORT_CREATE  3U
#define NOVA_SYS_SERVICE_REGISTER 4U
#define NOVA_SYS_SERVICE_LOOKUP   5U
#define NOVA_SYS_SERVICE_REMOVE   6U
#define NOVA_SYS_IPC_PORT_CLOSE   7U

#endif
