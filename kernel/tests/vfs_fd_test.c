#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vfs.h"
#include "vfs_fd.h"

int main(void) {
    struct nova_vfs_fd_table process_a, process_b;
    int32_t a, b, write_only, stale, replacement;
    char out[16] = {0};
    const char payload[] = "descriptor";

    assert(nova_vfs_init());
    nova_vfs_fd_table_init(&process_a);
    nova_vfs_fd_table_init(&process_b);

    a = nova_vfs_open(&process_a, "/ramfs/data.txt",
                      NOVA_VFS_OPEN_READ | NOVA_VFS_OPEN_WRITE |
                      NOVA_VFS_OPEN_CREATE);
    assert(a >= 0);
    assert(nova_vfs_write_fd(&process_a, a, payload, sizeof(payload)) ==
           (int64_t)sizeof(payload));
    assert(nova_vfs_seek_fd(&process_a, a, 0U));
    assert(nova_vfs_read_fd(&process_a, a, out, sizeof(out)) ==
           (int64_t)sizeof(payload));
    assert(memcmp(out, payload, sizeof(payload)) == 0);
    assert(nova_vfs_read_fd(&process_a, a, out, sizeof(out)) == 0);

    b = nova_vfs_open(&process_b, "/ramfs/data.txt", NOVA_VFS_OPEN_READ);
    assert(b >= 0);
    assert(nova_vfs_read_fd(&process_b, b, out, 4U) == 4);
    assert(nova_vfs_read_fd(&process_b, b, out, 4U) == 4);
    assert(nova_vfs_write_fd(&process_b, b, payload, 1U) == -1);
    write_only = nova_vfs_open(&process_a, "/ramfs/data.txt",
                               NOVA_VFS_OPEN_WRITE);
    assert(write_only >= 0);
    assert(nova_vfs_read_fd(&process_a, write_only, out, 1U) == -1);

    stale = a;
    assert(nova_vfs_close(&process_a, a));
    replacement = nova_vfs_open(&process_a, "/ramfs/data.txt",
                                NOVA_VFS_OPEN_READ);
    assert(replacement >= 0 && replacement != stale);
    assert(nova_vfs_read_fd(&process_a, stale, out, 1U) == -1);
    assert(nova_vfs_close(&process_a, replacement));
    assert(nova_vfs_close(&process_a, write_only));
    assert(nova_vfs_close(&process_b, b));
    assert(!nova_vfs_close(&process_b, b));

    assert(nova_vfs_remove("/ramfs/data.txt"));
    assert(nova_vfs_create("/ramfs/data.txt"));
    assert(nova_vfs_read_fd(&process_a, stale, out, 1U) == -1);
    puts("Nova OS VFS file descriptor tests passed.");
    return 0;
}
