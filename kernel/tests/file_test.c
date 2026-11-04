#include "file.h"
#include "vfs.h"

#include <assert.h>
#include <string.h>

int main(void) {
    const char payload[] = "Nova file descriptor";
    char result[sizeof(payload)] = {0};
    int64_t fd;
    assert(nova_vfs_init());
    assert(nova_file_open(10U, "/ramfs/notes.txt", 0U) == -2);
    fd = nova_file_open(10U, "/ramfs/notes.txt", NOVA_FILE_OPEN_CREATE);
    assert(fd == NOVA_FILE_FD_FIRST);
    assert(nova_file_write(10U, (int)fd, payload, sizeof(payload)) ==
           (int64_t)sizeof(payload));
    assert(nova_file_read(10U, (int)fd, result, sizeof(result)) == 0);
    assert(nova_file_close(10U, (int)fd) == 0);
    fd = nova_file_open(10U, "/ramfs/notes.txt", 0U);
    assert(fd == NOVA_FILE_FD_FIRST);
    assert(nova_file_read(10U, (int)fd, result, sizeof(result)) ==
           (int64_t)sizeof(result));
    assert(memcmp(payload, result, sizeof(payload)) == 0);
    assert(nova_file_read(11U, (int)fd, result, 1U) == -9);
    assert(nova_file_close(10U, (int)fd) == 0);
    assert(nova_file_close(10U, (int)fd) == -9);
    fd = nova_file_open(10U, "/ramfs/notes.txt", 0U);
    assert(fd == NOVA_FILE_FD_FIRST);
    nova_file_close_all(10U);
    assert(nova_file_read(10U, (int)fd, result, 1U) == -9);
    assert(nova_file_open(10U, "/ramfs/../escape", NOVA_FILE_OPEN_CREATE) == -22);
    return 0;
}
