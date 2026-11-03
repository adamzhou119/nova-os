#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fs.h"

int main(void) {
    struct nova_fs_stat stat;
    struct nova_fs_dirent entry;
    const char payload[] = "Nova OS filesystem";
    char output[32];
    size_t read_count = 0U;

    assert(nova_fs_init());
    assert(!nova_fs_create(""));
    assert(!nova_fs_create("../escape"));
    assert(!nova_fs_create("bad/name"));
    assert(!nova_fs_create("."));
    assert(nova_fs_create("welcome.txt"));
    assert(!nova_fs_create("welcome.txt"));
    assert(nova_fs_stat_path("welcome.txt", &stat));
    assert(stat.size == 0U);

    assert(nova_fs_write("welcome.txt", 0U, payload, sizeof(payload)));
    assert(nova_fs_stat_path("welcome.txt", &stat));
    assert(stat.size == sizeof(payload));
    memset(output, 0xA5, sizeof(output));
    assert(nova_fs_read("welcome.txt", 0U, output, sizeof(output),
                        &read_count));
    assert(read_count == sizeof(payload));
    assert(memcmp(output, payload, sizeof(payload)) == 0);

    assert(nova_fs_write("welcome.txt", 24U, "end", 3U));
    assert(nova_fs_read("welcome.txt", sizeof(payload), output, 16U,
                        &read_count));
    assert(read_count == 24U + 3U - sizeof(payload));
    assert(output[0] == 0U && output[1] == 0U && output[2] == 0U);

    assert(!nova_fs_write("welcome.txt", NOVA_FS_FILE_SIZE_MAX, "x", 1U));
    assert(nova_fs_read("welcome.txt", NOVA_FS_FILE_SIZE_MAX, output,
                        sizeof(output), &read_count));
    assert(read_count == 0U);
    assert(nova_fs_list(0U, &entry));
    assert(strcmp(entry.name, "welcome.txt") == 0);
    assert(!nova_fs_list(1U, &entry));
    assert(nova_fs_remove("welcome.txt"));
    assert(!nova_fs_stat_path("welcome.txt", &stat));

    puts("Nova OS RAM filesystem host tests passed.");
    return 0;
}
