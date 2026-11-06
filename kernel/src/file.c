#include "file.h"
#include "vfs.h"

struct nova_open_file {
    uint32_t owner;
    size_t offset;
    char path[NOVA_FILE_PATH_MAX + 1U];
    int used;
};

static struct nova_open_file open_files[NOVA_FILE_FD_MAX];

static int valid_owner(uint32_t owner) { return owner != 0U; }

static struct nova_open_file *lookup(uint32_t owner, int fd) {
    size_t slot;
    if (!valid_owner(owner) || fd < NOVA_FILE_FD_FIRST) return 0;
    slot = (size_t)(fd - NOVA_FILE_FD_FIRST);
    if (slot >= NOVA_FILE_FD_MAX || !open_files[slot].used ||
        open_files[slot].owner != owner) return 0;
    return &open_files[slot];
}

int64_t nova_file_open(uint32_t owner, const char *path, uint32_t flags) {
    struct nova_fs_stat stat;
    size_t length = 0U, i, slot;
    int exists;
    if (!valid_owner(owner) || path == 0 ||
        (flags & ~NOVA_FILE_OPEN_CREATE) != 0U) return -22;
    while (length <= NOVA_FILE_PATH_MAX && path[length] != '\0') ++length;
    if (length == 0U || length > NOVA_FILE_PATH_MAX) return -22;
    /*
     * Reserve descriptor capacity before a create operation. Otherwise a
     * failed open on a full descriptor table could leave a new file behind.
     * The kernel is currently single-core, so the free slot remains reserved
     * by this syscall until the descriptor is published below.
     */
    for (slot = 0U; slot < NOVA_FILE_FD_MAX; ++slot)
        if (!open_files[slot].used) break;
    if (slot == NOVA_FILE_FD_MAX) return -24;

    exists = nova_vfs_stat(path, &stat);
    if (!exists) {
        if ((flags & NOVA_FILE_OPEN_CREATE) == 0U) return -2;
        if (!nova_vfs_create(path)) return -22;
    }
    open_files[slot].owner = owner;
    open_files[slot].offset = 0U;
    for (i = 0U; i < length; ++i) open_files[slot].path[i] = path[i];
    open_files[slot].path[length] = '\0';
    open_files[slot].used = 1;
    return (int64_t)(NOVA_FILE_FD_FIRST + slot);
}

int64_t nova_file_close(uint32_t owner, int fd) {
    struct nova_open_file *file = lookup(owner, fd);
    size_t i;
    if (file == 0) return -9;
    for (i = 0U; i <= NOVA_FILE_PATH_MAX; ++i) file->path[i] = '\0';
    file->owner = 0U;
    file->offset = 0U;
    file->used = 0;
    return 0;
}

int64_t nova_file_read(uint32_t owner, int fd, void *buffer, size_t length) {
    struct nova_open_file *file = lookup(owner, fd);
    size_t count = 0U;
    if (file == 0) return -9;
    if (length > 256U || (buffer == 0 && length != 0U)) return -22;
    if (!nova_vfs_read(file->path, file->offset, buffer, length, &count))
        return -5;
    file->offset += count;
    return (int64_t)count;
}

int64_t nova_file_write(uint32_t owner, int fd, const void *buffer,
                        size_t length) {
    struct nova_open_file *file = lookup(owner, fd);
    if (file == 0) return -9;
    if (length > 256U || (buffer == 0 && length != 0U)) return -22;
    if (!nova_vfs_write(file->path, file->offset, buffer, length)) return -28;
    file->offset += length;
    return (int64_t)length;
}

void nova_file_close_all(uint32_t owner) {
    size_t i, j;
    if (!valid_owner(owner)) return;
    for (i = 0U; i < NOVA_FILE_FD_MAX; ++i) {
        if (!open_files[i].used || open_files[i].owner != owner) continue;
        for (j = 0U; j <= NOVA_FILE_PATH_MAX; ++j)
            open_files[i].path[j] = '\0';
        open_files[i].owner = 0U;
        open_files[i].offset = 0U;
        open_files[i].used = 0;
    }
}
