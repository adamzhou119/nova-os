#ifndef NOVA_FS_H
#define NOVA_FS_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_FS_NAME_MAX 31U
#define NOVA_FS_FILE_MAX 32U
#define NOVA_FS_FILE_SIZE_MAX 4096U

struct nova_fs_stat {
    size_t size;
    uint32_t generation;
};

struct nova_fs_dirent {
    char name[NOVA_FS_NAME_MAX + 1U];
    size_t size;
};

int nova_fs_init(void);
int nova_fs_create(const char *name);
int nova_fs_remove(const char *name);
int nova_fs_stat_path(const char *name, struct nova_fs_stat *stat);
int nova_fs_write(const char *name, size_t offset,
                  const void *data, size_t length);
int nova_fs_read(const char *name, size_t offset,
                 void *data, size_t capacity, size_t *bytes_read);
int nova_fs_list(size_t index, struct nova_fs_dirent *entry);

#endif
