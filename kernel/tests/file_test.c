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

    /* A full descriptor table must not create a file as a side effect. */
    {
        int descriptors[NOVA_FILE_FD_MAX];
        size_t i;
        struct nova_fs_stat stat;
        for (i = 0U; i < NOVA_FILE_FD_MAX; ++i) {
            descriptors[i] = (int)nova_file_open(10U, "/ramfs/notes.txt", 0U);
            assert(descriptors[i] == NOVA_FILE_FD_FIRST + (int)i);
        }
        assert(nova_file_open(10U, "/ramfs/no_slot.txt",
                              NOVA_FILE_OPEN_CREATE) == -24);
        assert(!nova_vfs_stat("/ramfs/no_slot.txt", &stat));
        nova_file_close_all(10U);
        assert(nova_file_read(10U, descriptors[0], result, 1U) == -9);
    }
    return 0;
}
