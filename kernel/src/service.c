#include "service.h"

#include "ipc.h"

#include <stddef.h>
#include <stdint.h>

#define NOVA_SERVICE_ERR_PERM  (-13)
#define NOVA_SERVICE_ERR_NOENT (-2)
#define NOVA_SERVICE_ERR_INVAL (-22)
#define NOVA_SERVICE_ERR_NOSPC (-28)
#define NOVA_SERVICE_ERR_EXIST (-17)

struct service_entry {
    uint32_t owner;
    uint32_t port;
    uint8_t used;
    char name[NOVA_SERVICE_NAME_MAX + 1U];
};

static struct service_entry entries[NOVA_SERVICE_MAX_ENTRIES];

static int valid_name_char(char value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') ||
           value == '.' || value == '_' || value == '-';
}

static int name_length(const char *name, size_t *length) {
    size_t i;
    if (name == 0 || length == 0) return 0;
    for (i = 0U; i <= NOVA_SERVICE_NAME_MAX; ++i) {
        if (name[i] == '\0') {
            if (i == 0U) return 0;
            *length = i;
            return 1;
        }
        if (!valid_name_char(name[i])) return 0;
    }
    return 0;
}

static int name_equal(const char *left, const char *right) {
    size_t i = 0U;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return 0;
        ++i;
    }
    return left[i] == right[i];
}

static int find_entry(const char *name) {
    size_t i;
    for (i = 0U; i < NOVA_SERVICE_MAX_ENTRIES; ++i)
        if (entries[i].used && name_equal(entries[i].name, name))
            return (int)i;
    return -1;
}

int nova_service_register(uint32_t owner, const char *name, uint32_t port) {
    size_t length, i, j;
    int free_slot = -1;
    if (owner == 0U || port == 0U || !name_length(name, &length))
        return NOVA_SERVICE_ERR_INVAL;
    if (!nova_ipc_port_is_owner(owner, port))
        return NOVA_SERVICE_ERR_PERM;
    if (find_entry(name) >= 0) return NOVA_SERVICE_ERR_EXIST;
    for (i = 0U; i < NOVA_SERVICE_MAX_ENTRIES; ++i) {
        if (!entries[i].used) {
            free_slot = (int)i;
            break;
        }
    }
    if (free_slot < 0) return NOVA_SERVICE_ERR_NOSPC;
    entries[free_slot].owner = owner;
    entries[free_slot].port = port;
    for (j = 0U; j < length; ++j)
        entries[free_slot].name[j] = name[j];
    entries[free_slot].name[length] = '\0';
    entries[free_slot].used = 1U;
    return 0;
}

int nova_service_unregister(uint32_t owner, const char *name) {
    size_t ignored;
    int index;
    if (owner == 0U || !name_length(name, &ignored))
        return NOVA_SERVICE_ERR_INVAL;
    index = find_entry(name);
    if (index < 0) return NOVA_SERVICE_ERR_NOENT;
    if (entries[index].owner != owner) return NOVA_SERVICE_ERR_PERM;
    entries[index].owner = 0U;
    entries[index].port = 0U;
    entries[index].used = 0U;
    entries[index].name[0] = '\0';
    return 0;
}

int64_t nova_service_lookup(const char *name) {
    size_t ignored;
    int index;
    if (!name_length(name, &ignored)) return NOVA_SERVICE_ERR_INVAL;
    index = find_entry(name);
    if (index < 0) return NOVA_SERVICE_ERR_NOENT;
    return (int64_t)entries[index].port;
}

void nova_service_close_owner(uint32_t owner) {
    size_t i;
    if (owner == 0U) return;
    for (i = 0U; i < NOVA_SERVICE_MAX_ENTRIES; ++i) {
        if (!entries[i].used || entries[i].owner != owner) continue;
        entries[i].owner = 0U;
        entries[i].port = 0U;
        entries[i].used = 0U;
        entries[i].name[0] = '\0';
    }
}
