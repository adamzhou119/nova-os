#ifndef NOVA_FILE_H
#define NOVA_FILE_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_FILE_FD_FIRST 3
#define NOVA_FILE_FD_MAX 32U
#define NOVA_FILE_PATH_MAX 39U
#define NOVA_FILE_OPEN_CREATE 1U

/* Per-task descriptor table; current implementation is single-core. */
int64_t nova_file_open(uint32_t owner, const char *path, uint32_t flags);
int64_t nova_file_close(uint32_t owner, int fd);
int64_t nova_file_read(uint32_t owner, int fd, void *buffer, size_t length);
int64_t nova_file_write(uint32_t owner, int fd, const void *buffer, size_t length);
void nova_file_close_all(uint32_t owner);

#endif
