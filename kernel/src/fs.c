#include "fs.h"

#include <stdint.h>

struct nova_fs_file {
    char name[NOVA_FS_NAME_MAX + 1U];
    uint8_t data[NOVA_FS_FILE_SIZE_MAX];
    size_t size;
    uint32_t generation;
    int used;
};

static struct nova_fs_file files[NOVA_FS_FILE_MAX];
static uint32_t next_generation = 1U;
static int initialized;

static size_t bounded_length(const char *text, size_t limit) {
    size_t length = 0U;
    if (text == 0) return limit + 1U;
    while (length <= limit && text[length] != '\0') ++length;
    return length;
}

static int valid_name(const char *name, size_t *length) {
    size_t i, n = bounded_length(name, NOVA_FS_NAME_MAX);
    if (n == 0U || n > NOVA_FS_NAME_MAX) return 0;
    if ((n == 1U && name[0] == '.') ||
        (n == 2U && name[0] == '.' && name[1] == '.')) return 0;
    for (i = 0U; i < n; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '-' || ch == '.'))
            return 0;
    }
    *length = n;
    return 1;
}

static int find_file(const char *name, size_t length) {
    size_t i;
    for (i = 0U; i < NOVA_FS_FILE_MAX; ++i) {
        size_t j;
        if (!files[i].used) continue;
        for (j = 0U; j < length && files[i].name[j] == name[j]; ++j) {}
        if (j == length && files[i].name[j] == '\0') return (int)i;
    }
    return -1;
}

int nova_fs_init(void) {
    size_t i, j;
    for (i = 0U; i < NOVA_FS_FILE_MAX; ++i) {
        files[i].used = 0;
        files[i].size = 0U;
        files[i].generation = 0U;
        for (j = 0U; j <= NOVA_FS_NAME_MAX; ++j) files[i].name[j] = '\0';
        for (j = 0U; j < NOVA_FS_FILE_SIZE_MAX; ++j) files[i].data[j] = 0U;
    }
    next_generation = 1U;
    initialized = 1;
    return 1;
}

int nova_fs_create(const char *name) {
    size_t length, i, j;
    if (!initialized || !valid_name(name, &length) ||
        find_file(name, length) >= 0 || next_generation == 0U)
        return 0;
    for (i = 0U; i < NOVA_FS_FILE_MAX; ++i)
        if (!files[i].used) break;
    if (i == NOVA_FS_FILE_MAX) return 0;
    for (j = 0U; j < length; ++j) files[i].name[j] = name[j];
    files[i].name[length] = '\0';
    files[i].size = 0U;
    files[i].generation = next_generation++;
    files[i].used = 1;
    return 1;
}

int nova_fs_remove(const char *name) {
    size_t length, i;
    int index;
    if (!initialized || !valid_name(name, &length)) return 0;
    index = find_file(name, length);
    if (index < 0) return 0;
    for (i = 0U; i <= NOVA_FS_NAME_MAX; ++i)
        files[index].name[i] = '\0';
    for (i = 0U; i < NOVA_FS_FILE_SIZE_MAX; ++i)
        files[index].data[i] = 0U;
    files[index].size = 0U;
    files[index].generation = 0U;
    files[index].used = 0;
    return 1;
}

int nova_fs_stat_path(const char *name, struct nova_fs_stat *stat) {
    size_t length;
    int index;
    if (!initialized || stat == 0 || !valid_name(name, &length)) return 0;
    index = find_file(name, length);
    if (index < 0) return 0;
    stat->size = files[index].size;
    stat->generation = files[index].generation;
    return 1;
}

int nova_fs_write(const char *name, size_t offset,
                  const void *data, size_t length) {
    size_t name_length, i;
    int index;
    if (!initialized || (data == 0 && length != 0U) ||
        !valid_name(name, &name_length) ||
        offset > NOVA_FS_FILE_SIZE_MAX ||
        length > NOVA_FS_FILE_SIZE_MAX - offset)
        return 0;
    index = find_file(name, name_length);
    if (index < 0) return 0;
    for (i = files[index].size; i < offset; ++i) files[index].data[i] = 0U;
    for (i = 0U; i < length; ++i)
        files[index].data[offset + i] = ((const uint8_t *)data)[i];
    if (offset + length > files[index].size)
        files[index].size = offset + length;
    return 1;
}

int nova_fs_read(const char *name, size_t offset, void *data,
                 size_t capacity, size_t *bytes_read) {
    size_t name_length, available, count, i;
    int index;
    if (!initialized || bytes_read == 0 ||
        (data == 0 && capacity != 0U) ||
        !valid_name(name, &name_length))
        return 0;
    index = find_file(name, name_length);
    if (index < 0) return 0;
    if (offset >= files[index].size) {
        *bytes_read = 0U;
        return 1;
    }
    available = files[index].size - offset;
    count = available < capacity ? available : capacity;
    for (i = 0U; i < count; ++i)
        ((uint8_t *)data)[i] = files[index].data[offset + i];
    *bytes_read = count;
    return 1;
}

int nova_fs_list(size_t index, struct nova_fs_dirent *entry) {
    size_t i, found = 0U, j;
    if (!initialized || entry == 0) return 0;
    for (i = 0U; i < NOVA_FS_FILE_MAX; ++i) {
        if (!files[i].used) continue;
        if (found++ != index) continue;
        for (j = 0U; j <= NOVA_FS_NAME_MAX; ++j)
            entry->name[j] = files[i].name[j];
        entry->size = files[i].size;
        return 1;
    }
    return 0;
}
