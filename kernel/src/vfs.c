#include "vfs.h"

/*
 * The first VFS mount is deliberately small: /ramfs is a flat namespace
 * backed by the in-memory filesystem. Keeping path parsing here means future
 * block-backed filesystems and syscall file descriptors do not need to know
 * about RAMFS's internal name rules.
 */
#define NOVA_VFS_MOUNT_PREFIX "/ramfs/"
#define NOVA_VFS_PREFIX_LENGTH 7U

static int vfs_ready;

static const char *resolve_ramfs_path(const char *path) {
    size_t i;
    if (path == 0 || !vfs_ready) return 0;
    for (i = 0U; i < NOVA_VFS_PREFIX_LENGTH; ++i)
        if (path[i] != NOVA_VFS_MOUNT_PREFIX[i]) return 0;
    path += NOVA_VFS_PREFIX_LENGTH;
    if (path[0] == '\0') return 0;
    for (i = 0U; path[i] != '\0'; ++i)
        if (path[i] == '/') return 0;
    return path;
}

int nova_vfs_init(void) {
    vfs_ready = 0;
    if (!nova_fs_init()) return 0;
    vfs_ready = 1;
    return 1;
}

int nova_vfs_create(const char *path) {
    const char *name = resolve_ramfs_path(path);
    return name != 0 && nova_fs_create(name);
}

int nova_vfs_remove(const char *path) {
    const char *name = resolve_ramfs_path(path);
    return name != 0 && nova_fs_remove(name);
}

int nova_vfs_stat(const char *path, struct nova_fs_stat *stat) {
    const char *name = resolve_ramfs_path(path);
    return name != 0 && nova_fs_stat_path(name, stat);
}

int nova_vfs_write(const char *path, size_t offset,
                   const void *data, size_t length) {
    const char *name = resolve_ramfs_path(path);
    return name != 0 && nova_fs_write(name, offset, data, length);
}

int nova_vfs_read(const char *path, size_t offset, void *data,
                  size_t capacity, size_t *bytes_read) {
    const char *name = resolve_ramfs_path(path);
    return name != 0 &&
           nova_fs_read(name, offset, data, capacity, bytes_read);
}

int nova_vfs_readdir(const char *path, size_t index,
                     struct nova_fs_dirent *entry) {
    size_t i;
    static const char mount_name[] = "/ramfs";
    if (!vfs_ready || path == 0 || entry == 0) return 0;
    for (i = 0U; i < sizeof(mount_name); ++i)
        if (path[i] != mount_name[i]) return 0;
    if (path[sizeof(mount_name) - 1U] == '\0')
        return nova_fs_list(index, entry);
    return 0;
}
