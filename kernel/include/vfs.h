#ifndef NOVA_VFS_H
#define NOVA_VFS_H

#include "fs.h"

/* Initial VFS namespace: one volatile RAMFS mounted at /ramfs. */
int nova_vfs_init(void);
int nova_vfs_create(const char *path);
int nova_vfs_remove(const char *path);
int nova_vfs_stat(const char *path, struct nova_fs_stat *stat);
int nova_vfs_write(const char *path, size_t offset,
                   const void *data, size_t length);
int nova_vfs_read(const char *path, size_t offset, void *data,
                  size_t capacity, size_t *bytes_read);
int nova_vfs_readdir(const char *path, size_t index,
                     struct nova_fs_dirent *entry);

#endif
