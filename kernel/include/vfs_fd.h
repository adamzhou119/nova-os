#ifndef NOVA_VFS_FD_H
#define NOVA_VFS_FD_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_VFS_FD_MAX 16U
#define NOVA_VFS_FD_PATH_MAX 39U

#define NOVA_VFS_OPEN_READ   0x01U
#define NOVA_VFS_OPEN_WRITE  0x02U
#define NOVA_VFS_OPEN_CREATE 0x04U

struct nova_vfs_fd_entry {
    char path[NOVA_VFS_FD_PATH_MAX + 1U];
    size_t offset;
    uint32_t file_generation;
    uint32_t handle_generation;
    uint8_t flags;
    uint8_t used;
};

struct nova_vfs_fd_table {
    struct nova_vfs_fd_entry entries[NOVA_VFS_FD_MAX];
};

void nova_vfs_fd_table_init(struct nova_vfs_fd_table *table);
int32_t nova_vfs_open(struct nova_vfs_fd_table *table,
                      const char *path, uint8_t flags);
int nova_vfs_close(struct nova_vfs_fd_table *table, int32_t fd);
int64_t nova_vfs_read_fd(struct nova_vfs_fd_table *table, int32_t fd,
                         void *buffer, size_t capacity);
int64_t nova_vfs_write_fd(struct nova_vfs_fd_table *table, int32_t fd,
                          const void *buffer, size_t length);
int nova_vfs_seek_fd(struct nova_vfs_fd_table *table, int32_t fd,
                     size_t offset);

#endif
