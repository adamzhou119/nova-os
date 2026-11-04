#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vfs.h"

int main(void) {
    struct nova_fs_stat stat;
    struct nova_fs_dirent entry;
    const char message[] = "VFS path routing works";
    char result[32];
    size_t count = 0U;

    assert(nova_vfs_init());
    assert(!nova_vfs_create("ramfs/a"));
    assert(!nova_vfs_create("/r"));
    assert(!nova_vfs_create("/ramfs/"));
    assert(!nova_vfs_create("/ramfs/../escape"));
    assert(!nova_vfs_create("/ramfs/sub/file"));
    assert(!nova_vfs_create("/other/file"));
    assert(nova_vfs_create("/ramfs/hello.txt"));
    assert(nova_vfs_write("/ramfs/hello.txt", 0U, message,
                          sizeof(message)));
    assert(nova_vfs_stat("/ramfs/hello.txt", &stat));
    assert(stat.size == sizeof(message));
    assert(nova_vfs_read("/ramfs/hello.txt", 0U, result,
                         sizeof(result), &count));
    assert(count == sizeof(message));
    assert(memcmp(result, message, count) == 0);
    assert(nova_vfs_readdir("/ramfs", 0U, &entry));
    assert(strcmp(entry.name, "hello.txt") == 0);
    assert(!nova_vfs_readdir("/ramfs/sub", 0U, &entry));
    assert(nova_vfs_remove("/ramfs/hello.txt"));
    assert(!nova_vfs_stat("/ramfs/hello.txt", &stat));

    puts("Nova OS VFS RAMFS mount tests passed.");
    return 0;
}
