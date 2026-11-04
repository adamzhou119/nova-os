#include "vfs_fd.h"

#include "vfs.h"

#define NOVA_VFS_FD_SLOT_BITS 8U
#define NOVA_VFS_FD_SLOT_MASK ((1U << NOVA_VFS_FD_SLOT_BITS) - 1U)
#define NOVA_VFS_FD_GENERATION_MAX 0x007FFFFFU

static int path_length(const char *path, size_t *length) {
    size_t n = 0U;
    if (path == 0 || length == 0) return 0;
    while (n <= NOVA_VFS_FD_PATH_MAX && path[n] != '\0') ++n;
    if (n == 0U || n > NOVA_VFS_FD_PATH_MAX) return 0;
    *length = n;
    return 1;
}

static int decode_fd(struct nova_vfs_fd_table *table, int32_t fd,
                     struct nova_vfs_fd_entry **entry) {
    uint32_t value, slot, generation;
    struct nova_vfs_fd_entry *candidate;
    if (table == 0 || fd < 0 || entry == 0) return 0;
    value = (uint32_t)fd;
    slot = value & NOVA_VFS_FD_SLOT_MASK;
    generation = value >> NOVA_VFS_FD_SLOT_BITS;
    if (slot >= NOVA_VFS_FD_MAX || generation == 0U) return 0;
    candidate = &table->entries[slot];
    if (!candidate->used || candidate->handle_generation != generation)
        return 0;
    *entry = candidate;
    return 1;
}

static int file_still_matches(const struct nova_vfs_fd_entry *entry) {
    struct nova_fs_stat stat;
    return nova_vfs_stat(entry->path, &stat) &&
           stat.generation == entry->file_generation;
}

void nova_vfs_fd_table_init(struct nova_vfs_fd_table *table) {
    size_t i, j;
    if (table == 0) return;
    for (i = 0U; i < NOVA_VFS_FD_MAX; ++i) {
        struct nova_vfs_fd_entry *entry = &table->entries[i];
        for (j = 0U; j <= NOVA_VFS_FD_PATH_MAX; ++j) entry->path[j] = '\0';
        entry->offset = 0U;
        entry->file_generation = 0U;
        entry->handle_generation = 0U;
        entry->flags = 0U;
        entry->used = 0U;
    }
}

int32_t nova_vfs_open(struct nova_vfs_fd_table *table,
                      const char *path, uint8_t flags) {
    struct nova_fs_stat stat;
    size_t length, i, j;
    struct nova_vfs_fd_entry *entry;
    uint32_t generation;
    if (table == 0 || !path_length(path, &length) ||
        (flags & (NOVA_VFS_OPEN_READ | NOVA_VFS_OPEN_WRITE)) == 0U ||
        (flags & ~(NOVA_VFS_OPEN_READ | NOVA_VFS_OPEN_WRITE |
                   NOVA_VFS_OPEN_CREATE)) != 0U)
        return -1;
    if (!nova_vfs_stat(path, &stat)) {
        if ((flags & NOVA_VFS_OPEN_CREATE) == 0U ||
            !nova_vfs_create(path) || !nova_vfs_stat(path, &stat))
            return -1;
    }
    for (i = 0U; i < NOVA_VFS_FD_MAX; ++i) {
        if (!table->entries[i].used &&
            table->entries[i].handle_generation < NOVA_VFS_FD_GENERATION_MAX)
            break;
    }
    if (i == NOVA_VFS_FD_MAX) return -1;
    entry = &table->entries[i];
    generation = entry->handle_generation + 1U;
    for (j = 0U; j < length; ++j) entry->path[j] = path[j];
    entry->path[length] = '\0';
    entry->offset = 0U;
    entry->file_generation = stat.generation;
    entry->handle_generation = generation;
    entry->flags = flags;
    entry->used = 1U;
    return (int32_t)((generation << NOVA_VFS_FD_SLOT_BITS) | (uint32_t)i);
}

int nova_vfs_close(struct nova_vfs_fd_table *table, int32_t fd) {
    struct nova_vfs_fd_entry *entry;
    size_t i;
    if (!decode_fd(table, fd, &entry)) return 0;
    for (i = 0U; i <= NOVA_VFS_FD_PATH_MAX; ++i) entry->path[i] = '\0';
    entry->offset = 0U;
    entry->file_generation = 0U;
    entry->flags = 0U;
    entry->used = 0U;
    return 1;
}

int64_t nova_vfs_read_fd(struct nova_vfs_fd_table *table, int32_t fd,
                         void *buffer, size_t capacity) {
    struct nova_vfs_fd_entry *entry;
    size_t count = 0U;
    if (!decode_fd(table, fd, &entry) ||
        (entry->flags & NOVA_VFS_OPEN_READ) == 0U ||
        (buffer == 0 && capacity != 0U) || !file_still_matches(entry))
        return -1;
    if (!nova_vfs_read(entry->path, entry->offset, buffer, capacity, &count))
        return -1;
    entry->offset += count;
    return (int64_t)count;
}

int64_t nova_vfs_write_fd(struct nova_vfs_fd_table *table, int32_t fd,
                          const void *buffer, size_t length) {
    struct nova_vfs_fd_entry *entry;
    struct nova_fs_stat stat;
    if (!decode_fd(table, fd, &entry) ||
        (entry->flags & NOVA_VFS_OPEN_WRITE) == 0U ||
        (buffer == 0 && length != 0U) || !file_still_matches(entry) ||
        !nova_vfs_stat(entry->path, &stat) ||
        entry->offset > NOVA_FS_FILE_SIZE_MAX ||
        length > NOVA_FS_FILE_SIZE_MAX - entry->offset ||
        !nova_vfs_write(entry->path, entry->offset, buffer, length))
        return -1;
    entry->offset += length;
    return (int64_t)length;
}

int nova_vfs_seek_fd(struct nova_vfs_fd_table *table, int32_t fd,
                     size_t offset) {
    struct nova_vfs_fd_entry *entry;
    if (!decode_fd(table, fd, &entry) || !file_still_matches(entry) ||
        offset > NOVA_FS_FILE_SIZE_MAX)
        return 0;
    entry->offset = offset;
    return 1;
}
