#include "fdt.h"

#define FDT_MAGIC 0xd00dfeedU
#define FDT_HEADER_SIZE 40U
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U
#define FDT_MAX_DEPTH 16U

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

/* Returns 1 for enabled, 0 for a well-formed disabled/unknown state,
 * and -1 when the property is not exactly one NUL-terminated string. */
static int fdt_node_status_enabled(const uint8_t *status, uint32_t length) {
    uint32_t i;
    if (status == NULL) return 1; /* Device-tree default is enabled. */
    if (length <= 1U || status[length - 1U] != 0U) return -1;
    for (i = 0U; i + 1U < length; ++i)
        if (status[i] == 0U) return -1;
    if (length == 5U && status[0] == 'o' && status[1] == 'k' &&
        status[2] == 'a' && status[3] == 'y')
        return 1;
    if (length == 3U && status[0] == 'o' && status[1] == 'k')
        return 1;
    return 0;
}

static int fdt_string_equals(const uint8_t *strings, uint32_t strings_size,
                             uint32_t offset, const char *expected) {
    size_t i = 0;
    if (offset >= strings_size) return 0;
    while (i < strings_size - offset && expected[i] != 0) {
        if ((char)strings[offset + i] != expected[i]) return 0;
        ++i;
    }
    return i < strings_size - offset && strings[offset + i] == 0 &&
           expected[i] == 0;
}

static int node_is_memory(const char *name) {
    static const char prefix[] = "memory";
    size_t i;
    for (i = 0; i < sizeof(prefix) - 1U; ++i) {
        if (name[i] != prefix[i]) return 0;
    }
    return name[i] == '\0' || name[i] == '@';
}

static int read_cells(const uint8_t *data, uint32_t cells, uint64_t *value) {
    uint32_t i;
    uint64_t result = 0;
    if (cells == 0 || cells > 2U) return 0;
    for (i = 0; i < cells; ++i) {
        result = (result << 32) | read_be32(data + i * 4U);
    }
    *value = result;
    return 1;
}

int nova_fdt_memory_regions(uintptr_t dtb_address,
                            struct nova_memory_region *regions,
                            size_t capacity) {
    const uint8_t *base = (const uint8_t *)dtb_address;
    const uint8_t *structure;
    const uint8_t *strings;
    uint32_t total_size;
    uint32_t struct_offset;
    uint32_t strings_offset;
    uint32_t struct_size;
    uint32_t strings_size;
    uint32_t cursor = 0;
    int depth = -1;
    uint32_t address_cells[FDT_MAX_DEPTH];
    uint32_t size_cells[FDT_MAX_DEPTH];
    int memory_node[FDT_MAX_DEPTH];
    const uint8_t *reg_data[FDT_MAX_DEPTH];
    uint32_t reg_length[FDT_MAX_DEPTH];
    size_t found = 0;

    if (dtb_address == 0 || regions == NULL || capacity == 0) return -1;
    if (read_be32(base) != FDT_MAGIC) return -2;

    total_size = read_be32(base + 4U);
    struct_offset = read_be32(base + 8U);
    strings_offset = read_be32(base + 12U);
    strings_size = read_be32(base + 32U);
    struct_size = read_be32(base + 36U);

    if (total_size < FDT_HEADER_SIZE ||
        struct_offset > total_size || struct_size > total_size - struct_offset ||
        strings_offset > total_size || strings_size > total_size - strings_offset)
        return -3;

    structure = base + struct_offset;
    strings = base + strings_offset;

    while (cursor + 4U <= struct_size) {
        uint32_t token = read_be32(structure + cursor);
        cursor += 4U;

        if (token == FDT_BEGIN_NODE) {
            uint32_t name_start = cursor;
            uint32_t name_length;
            if (depth + 1 >= (int)FDT_MAX_DEPTH) return -4;
            while (cursor < struct_size && structure[cursor] != 0) ++cursor;
            if (cursor >= struct_size) return -5;
            name_length = cursor - name_start;
            ++cursor;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -6;

            ++depth;
            address_cells[depth] = depth == 0 ? 2U : address_cells[depth - 1];
            size_cells[depth] = depth == 0 ? 1U : size_cells[depth - 1];
            memory_node[depth] = node_is_memory((const char *)(structure + name_start));
            reg_data[depth] = NULL;
            reg_length[depth] = 0;
            (void)name_length;
        } else if (token == FDT_END_NODE) {
            if (depth < 0) return -7;
            if (memory_node[depth] && reg_data[depth] != NULL) {
                uint32_t tuple_cells = address_cells[depth] + size_cells[depth];
                uint32_t tuple_bytes;
                uint32_t pos = 0;
                if (address_cells[depth] == 0 || size_cells[depth] == 0 ||
                    tuple_cells > 4U) return -8;
                tuple_bytes = tuple_cells * 4U;
                if (reg_length[depth] == 0 ||
                    reg_length[depth] % tuple_bytes != 0) return -9;

                while (pos < reg_length[depth]) {
                    uint64_t start;
                    uint64_t size;
                    if (!read_cells(reg_data[depth] + pos,
                                    address_cells[depth], &start) ||
                        !read_cells(reg_data[depth] + pos +
                                    address_cells[depth] * 4U,
                                    size_cells[depth], &size))
                        return -10;
                    if (size != 0 && start <= UINT64_MAX - size) {
                        if (found < capacity) {
                            regions[found].start = start;
                            regions[found].size = size;
                        }
                        ++found;
                    }
                    pos += tuple_bytes;
                }
            }
            --depth;
        } else if (token == FDT_PROP) {
            uint32_t length;
            uint32_t name_offset;
            const uint8_t *data;
            if (depth < 0 || cursor + 8U > struct_size) return -11;
            length = read_be32(structure + cursor);
            name_offset = read_be32(structure + cursor + 4U);
            cursor += 8U;
            if (length > struct_size - cursor || name_offset >= strings_size)
                return -12;
            data = structure + cursor;

            if (fdt_string_equals(strings, strings_size, name_offset, "#address-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells > 4U) return -13;
                address_cells[depth] = cells;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "#size-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells > 4U) return -14;
                size_cells[depth] = cells;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "device_type") &&
                       length >= 7U && data[0] == 'm' && data[1] == 'e' &&
                       data[2] == 'm' && data[3] == 'o' && data[4] == 'r' &&
                       data[5] == 'y' && data[6] == 0) {
                memory_node[depth] = 1;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "reg")) {
                reg_data[depth] = data;
                reg_length[depth] = length;
            }

            cursor += length;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -15;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            return found > capacity ? -1 : (int)found;
        } else {
            return -16;
        }
    }

    return -17;
}


static uint64_t read_be64(const uint8_t *p) {
    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4U);
}

static int append_reserved(struct nova_memory_region *regions,
                           size_t capacity, size_t *found,
                           uint64_t start, uint64_t size) {
    if (size == 0) return 1;
    if (start > UINT64_MAX - size || *found >= capacity) return 0;
    regions[*found].start = start;
    regions[*found].size = size;
    ++*found;
    return 1;
}

int nova_fdt_reserved_regions(uintptr_t dtb_address,
                              struct nova_memory_region *regions,
                              size_t capacity) {
    const uint8_t *base = (const uint8_t *)dtb_address;
    const uint8_t *structure;
    const uint8_t *strings;
    uint32_t total_size, reserve_offset, struct_offset, strings_offset;
    uint32_t struct_size, strings_size, cursor = 0;
    uint32_t address_cells[FDT_MAX_DEPTH], size_cells[FDT_MAX_DEPTH];
    uint32_t reg_length[FDT_MAX_DEPTH];
    const uint8_t *reg_data[FDT_MAX_DEPTH];
    int reserved_root[FDT_MAX_DEPTH], reserved_child[FDT_MAX_DEPTH];
    int depth = -1;
    size_t found = 0;
    uint32_t reserve_cursor;

    if (dtb_address == 0 || regions == NULL || capacity == 0) return -1;
    if (read_be32(base) != FDT_MAGIC) return -2;
    total_size = read_be32(base + 4U);
    reserve_offset = read_be32(base + 16U);
    struct_offset = read_be32(base + 8U);
    strings_offset = read_be32(base + 12U);
    strings_size = read_be32(base + 32U);
    struct_size = read_be32(base + 36U);
    if (total_size < FDT_HEADER_SIZE ||
        reserve_offset > total_size || total_size - reserve_offset < 16U ||
        struct_offset > total_size || struct_size > total_size - struct_offset ||
        strings_offset > total_size || strings_size > total_size - strings_offset)
        return -3;

    reserve_cursor = reserve_offset;
    for (;;) {
        uint64_t start, size;
        if (reserve_cursor > total_size || total_size - reserve_cursor < 16U)
            return -4;
        start = read_be64(base + reserve_cursor);
        size = read_be64(base + reserve_cursor + 8U);
        reserve_cursor += 16U;
        if (start == 0 && size == 0) break;
        if (!append_reserved(regions, capacity, &found, start, size))
            return -5;
    }

    structure = base + struct_offset;
    strings = base + strings_offset;
    while (cursor + 4U <= struct_size) {
        uint32_t token = read_be32(structure + cursor);
        cursor += 4U;
        if (token == FDT_BEGIN_NODE) {
            uint32_t name_start = cursor;
            uint32_t name_length;
            int parent_reserved_root = depth >= 0 && reserved_root[depth];
            int is_reserved_memory;
            uint32_t i;
            if (depth + 1 >= (int)FDT_MAX_DEPTH) return -6;
            while (cursor < struct_size && structure[cursor] != 0) ++cursor;
            if (cursor >= struct_size) return -7;
            name_length = cursor - name_start;
            is_reserved_memory = name_length == 15U;
            for (i = 0; is_reserved_memory && i < 15U; ++i) {
                static const char reserved_name[] = "reserved-memory";
                if ((char)structure[name_start + i] != reserved_name[i])
                    is_reserved_memory = 0;
            }
            ++cursor;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -8;
            ++depth;
            address_cells[depth] = depth == 0 ? 2U : address_cells[depth - 1];
            size_cells[depth] = depth == 0 ? 1U : size_cells[depth - 1];
            reserved_root[depth] = is_reserved_memory;
            reserved_child[depth] = parent_reserved_root;
            reg_data[depth] = NULL;
            reg_length[depth] = 0;
        } else if (token == FDT_END_NODE) {
            if (depth < 0) return -9;
            if (reserved_child[depth] && reg_data[depth] != NULL) {
                uint32_t ac = address_cells[depth];
                uint32_t sc = size_cells[depth];
                uint32_t tuple_cells = ac + sc;
                uint32_t tuple_bytes, pos = 0;
                if (ac == 0 || ac > 2U || sc == 0 || sc > 2U ||
                    tuple_cells > 4U) return -10;
                tuple_bytes = tuple_cells * 4U;
                if (reg_length[depth] == 0 ||
                    reg_length[depth] % tuple_bytes != 0) return -11;
                while (pos < reg_length[depth]) {
                    uint64_t start, size;
                    if (!read_cells(reg_data[depth] + pos, ac, &start) ||
                        !read_cells(reg_data[depth] + pos + ac * 4U, sc, &size) ||
                        !append_reserved(regions, capacity, &found, start, size))
                        return -12;
                    pos += tuple_bytes;
                }
            }
            --depth;
        } else if (token == FDT_PROP) {
            uint32_t length, name_offset;
            const uint8_t *data;
            if (depth < 0 || cursor + 8U > struct_size) return -13;
            length = read_be32(structure + cursor);
            name_offset = read_be32(structure + cursor + 4U);
            cursor += 8U;
            if (length > struct_size - cursor || name_offset >= strings_size)
                return -14;
            data = structure + cursor;
            if (fdt_string_equals(strings, strings_size, name_offset,
                                  "#address-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells > 4U) return -15;
                address_cells[depth] = cells;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "#size-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells > 4U) return -16;
                size_cells[depth] = cells;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "reg")) {
                reg_data[depth] = data;
                reg_length[depth] = length;
            }
            cursor += length;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -17;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            return (int)found;
        } else {
            return -18;
        }
    }
    return -19;
}


size_t nova_fdt_total_size(uintptr_t dtb_address) {
    const uint8_t *base = (const uint8_t *)dtb_address;
    uint32_t total;
    if (dtb_address == 0 || read_be32(base) != FDT_MAGIC) return 0;
    total = read_be32(base + 4U);
    if (total < FDT_HEADER_SIZE) return 0;
    return (size_t)total;
}


/* Match one entry in a NUL-separated FDT compatible string list. */
static int compatible_list_contains(const uint8_t *data, uint32_t length,
                                    const char *expected) {
    uint32_t pos = 0;
    size_t expected_length = 0;
    while (expected[expected_length] != '\0') ++expected_length;
    if (expected_length == 0U) return 0;
    while (pos < length) {
        uint32_t start = pos;
        while (pos < length && data[pos] != 0U) ++pos;
        if (pos == length) return 0;
        if ((size_t)(pos - start) == expected_length) {
            size_t i;
            for (i = 0; i < expected_length; ++i)
                if (data[start + i] != (uint8_t)expected[i]) break;
            if (i == expected_length) return 1;
        }
        ++pos;
    }
    return 0;
}

static int translate_fdt_bus_address(
    uint64_t *address, uint64_t device_size, int bus_depth,
    const uint32_t *parent_address_cells,
    const uint32_t *child_address_cells,
    const uint32_t *child_size_cells,
    const uint8_t *const *ranges_data,
    const uint32_t *ranges_length) {
    int depth;
    for (depth = bus_depth; depth > 0; --depth) {
        uint32_t child_ac = child_address_cells[depth];
        uint32_t parent_ac = parent_address_cells[depth];
        uint32_t size_cells = child_size_cells[depth];
        uint32_t tuple_cells, tuple_bytes, pos;
        int matched = 0;
        if (ranges_data[depth] == NULL) return 0;
        if (ranges_length[depth] == 0U) continue; /* Empty ranges: identity. */
        if (child_ac == 0U || child_ac > 2U || parent_ac == 0U ||
            parent_ac > 2U || size_cells == 0U || size_cells > 2U)
            return 0;
        tuple_cells = child_ac + parent_ac + size_cells;
        tuple_bytes = tuple_cells * 4U;
        if (ranges_length[depth] == 0U ||
            ranges_length[depth] % tuple_bytes != 0U) return 0;
        for (pos = 0U; pos < ranges_length[depth]; pos += tuple_bytes) {
            uint64_t child_base, parent_base, span, offset;
            const uint8_t *tuple = ranges_data[depth] + pos;
            if (!read_cells(tuple, child_ac, &child_base) ||
                !read_cells(tuple + child_ac * 4U, parent_ac, &parent_base) ||
                !read_cells(tuple + (child_ac + parent_ac) * 4U,
                            size_cells, &span) || span == 0U ||
                child_base > UINT64_MAX - span ||
                parent_base > UINT64_MAX - span)
                return 0;
            if (*address < child_base) continue;
            offset = *address - child_base;
            if (offset >= span || device_size > span - offset) continue;
            if (parent_base > UINT64_MAX - offset) return 0;
            *address = parent_base + offset;
            matched = 1;
            break;
        }
        if (!matched) return 0;
    }
    return 1;
}

static int fdt_find_device_reg(uintptr_t dtb_address, const char *compatible,
                               size_t instance_index, size_t reg_index,
                               struct nova_fdt_device *device) {
    const uint8_t *base = (const uint8_t *)dtb_address;
    const uint8_t *structure;
    const uint8_t *strings;
    uint32_t total_size, struct_offset, strings_offset;
    uint32_t struct_size, strings_size, cursor = 0;
    uint32_t parent_address_cells[FDT_MAX_DEPTH];
    uint32_t parent_size_cells[FDT_MAX_DEPTH];
    uint32_t child_address_cells[FDT_MAX_DEPTH];
    uint32_t child_size_cells[FDT_MAX_DEPTH];
    const uint8_t *compatible_data[FDT_MAX_DEPTH];
    uint32_t compatible_length[FDT_MAX_DEPTH];
    const uint8_t *reg_data[FDT_MAX_DEPTH];
    uint32_t reg_length[FDT_MAX_DEPTH];
    const uint8_t *ranges_data[FDT_MAX_DEPTH];
    uint32_t ranges_length[FDT_MAX_DEPTH];
    const uint8_t *status_data[FDT_MAX_DEPTH];
    uint32_t status_length[FDT_MAX_DEPTH];
    int depth = -1;
    size_t matched_instances = 0U;

    if (dtb_address == 0U || compatible == NULL || device == NULL ||
        compatible[0] == '\0') return -1;
    if (read_be32(base) != FDT_MAGIC) return -2;
    total_size = read_be32(base + 4U);
    struct_offset = read_be32(base + 8U);
    strings_offset = read_be32(base + 12U);
    strings_size = read_be32(base + 32U);
    struct_size = read_be32(base + 36U);
    if (total_size < FDT_HEADER_SIZE ||
        struct_offset > total_size || struct_size > total_size - struct_offset ||
        strings_offset > total_size || strings_size > total_size - strings_offset)
        return -3;

    structure = base + struct_offset;
    strings = base + strings_offset;
    while (cursor + 4U <= struct_size) {
        uint32_t token = read_be32(structure + cursor);
        cursor += 4U;
        if (token == FDT_BEGIN_NODE) {
            if (depth + 1 >= (int)FDT_MAX_DEPTH) return -4;
            while (cursor < struct_size && structure[cursor] != 0U) ++cursor;
            if (cursor >= struct_size) return -5;
            ++cursor;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -6;
            ++depth;
            parent_address_cells[depth] =
                depth == 0 ? 2U : child_address_cells[depth - 1];
            parent_size_cells[depth] =
                depth == 0 ? 1U : child_size_cells[depth - 1];
            child_address_cells[depth] = parent_address_cells[depth];
            child_size_cells[depth] = parent_size_cells[depth];
            compatible_data[depth] = NULL;
            compatible_length[depth] = 0U;
            reg_data[depth] = NULL;
            reg_length[depth] = 0U;
            ranges_data[depth] = NULL;
            ranges_length[depth] = 0U;
            status_data[depth] = NULL;
            status_length[depth] = 0U;
        } else if (token == FDT_END_NODE) {
            uint32_t ac, sc, tuple_bytes;
            uint64_t address, size;
            if (depth < 0) return -7;
            if (compatible_data[depth] != NULL &&
                compatible_list_contains(compatible_data[depth],
                                          compatible_length[depth],
                                          compatible) &&
                reg_data[depth] != NULL) {
                int enabled = fdt_node_status_enabled(status_data[depth],
                                                      status_length[depth]);
                if (enabled < 0) return -18;
                if (enabled == 1 && matched_instances++ != instance_index) {
                    --depth;
                    continue;
                }
                ac = parent_address_cells[depth];
                sc = parent_size_cells[depth];
                if (ac == 0U || ac > 2U || sc == 0U || sc > 2U)
                    return -8;
                tuple_bytes = (ac + sc) * 4U;
                if (reg_length[depth] < tuple_bytes ||
                    reg_length[depth] % tuple_bytes != 0U)
                    return -9;
                if (reg_index >= reg_length[depth] / tuple_bytes) return 0;
                if (!read_cells(reg_data[depth] + reg_index * tuple_bytes, ac, &address) ||
                    !read_cells(reg_data[depth] + reg_index * tuple_bytes + ac * 4U, sc, &size) ||
                    size == 0U || address > UINT64_MAX - size ||
                    address > UINTPTR_MAX || size > SIZE_MAX ||
                    !translate_fdt_bus_address(&address, size, depth - 1,
                        parent_address_cells, child_address_cells,
                        child_size_cells, ranges_data, ranges_length) ||
                    address > UINTPTR_MAX || address > UINT64_MAX - size)
                    return -10;
                device->mmio_base = (uintptr_t)address;
                device->mmio_size = (size_t)size;
                return 1;
            }
            --depth;
        } else if (token == FDT_PROP) {
            uint32_t length, name_offset;
            const uint8_t *data;
            if (depth < 0 || cursor + 8U > struct_size) return -11;
            length = read_be32(structure + cursor);
            name_offset = read_be32(structure + cursor + 4U);
            cursor += 8U;
            if (length > struct_size - cursor || name_offset >= strings_size)
                return -12;
            data = structure + cursor;
            if (fdt_string_equals(strings, strings_size, name_offset,
                                  "compatible")) {
                compatible_data[depth] = data;
                compatible_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "reg")) {
                reg_data[depth] = data;
                reg_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "ranges")) {
                ranges_data[depth] = data;
                ranges_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "status")) {
                status_data[depth] = data;
                status_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "#address-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells > 2U) return -13;
                child_address_cells[depth] = cells;
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "#size-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells > 2U) return -14;
                child_size_cells[depth] = cells;
            }
            cursor += length;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -15;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            return 0;
        } else {
            return -16;
        }
    }
    return -17;
}


int nova_fdt_find_device(uintptr_t dtb_address, const char *compatible,
                         struct nova_fdt_device *device) {
    return fdt_find_device_reg(dtb_address, compatible, 0U, 0U, device);
}

int nova_fdt_find_device_reg(uintptr_t dtb_address, const char *compatible,
                             size_t reg_index, struct nova_fdt_device *device) {
    return fdt_find_device_reg(dtb_address, compatible, 0U, reg_index, device);
}

int nova_fdt_find_device_instance(uintptr_t dtb_address, const char *compatible,
                                  size_t instance_index, size_t reg_index,
                                  struct nova_fdt_device *device) {
    return fdt_find_device_reg(dtb_address, compatible, instance_index,
                               reg_index, device);
}


int nova_fdt_decode_gicv2_interrupt(uint32_t type, uint32_t number,
                                    uint32_t flags, uint32_t *interrupt_id) {
    uint32_t id;
    if (interrupt_id == NULL || (flags != 1U && flags != 2U &&
                                 flags != 4U && flags != 8U))
        return 0;
    if (type == 0U) { /* SPI: INTID = number + 32. */
        if (number > 987U) return 0; /* GICv2 INTIDs end at 1019. */
        id = number + 32U;
    } else if (type == 1U) { /* PPI: INTID = number + 16. */
        if (number > 15U) return 0;
        id = number + 16U;
    } else {
        return 0;
    }
    *interrupt_id = id;
    return 1;
}


#define FDT_MAX_GIC_CONTROLLERS 16U

static int fdt_find_gicv2_irq_instance(uintptr_t dtb_address,
                                       const char *compatible,
                                       size_t instance_index,
                                       uint32_t *interrupt_id) {
    const uint8_t *base = (const uint8_t *)dtb_address;
    const uint8_t *structure;
    const uint8_t *strings;
    uint32_t total_size, struct_offset, strings_offset;
    uint32_t struct_size, strings_size, cursor = 0U;
    uint32_t current_parent[FDT_MAX_DEPTH];
    uint32_t node_phandle[FDT_MAX_DEPTH];
    uint32_t interrupt_cells[FDT_MAX_DEPTH];
    int node_is_interrupt_controller[FDT_MAX_DEPTH];
    const uint8_t *node_compatible[FDT_MAX_DEPTH];
    uint32_t node_compatible_length[FDT_MAX_DEPTH];
    const uint8_t *node_interrupts[FDT_MAX_DEPTH];
    uint32_t node_interrupts_length[FDT_MAX_DEPTH];
    const uint8_t *node_status[FDT_MAX_DEPTH];
    uint32_t node_status_length[FDT_MAX_DEPTH];
    int node_has_reg[FDT_MAX_DEPTH];
    uint32_t controller_phandles[FDT_MAX_GIC_CONTROLLERS];
    uint32_t controller_cells[FDT_MAX_GIC_CONTROLLERS];
    uint32_t controller_count = 0U;
    uint32_t target_parent = 0U;
    int target_found = 0;
    size_t matched_targets = 0U;
    const uint8_t *target_interrupts = NULL;
    uint32_t target_interrupts_length = 0U;
    int depth = -1;
    uint32_t i;

    if (dtb_address == 0U || compatible == NULL || compatible[0] == '\0' ||
        interrupt_id == NULL) return -1;
    if (read_be32(base) != FDT_MAGIC) return -2;
    total_size = read_be32(base + 4U);
    struct_offset = read_be32(base + 8U);
    strings_offset = read_be32(base + 12U);
    strings_size = read_be32(base + 32U);
    struct_size = read_be32(base + 36U);
    if (total_size < FDT_HEADER_SIZE ||
        struct_offset > total_size || struct_size > total_size - struct_offset ||
        strings_offset > total_size || strings_size > total_size - strings_offset)
        return -3;

    structure = base + struct_offset;
    strings = base + strings_offset;
    while (cursor + 4U <= struct_size) {
        uint32_t token = read_be32(structure + cursor);
        cursor += 4U;
        if (token == FDT_BEGIN_NODE) {
            if (depth + 1 >= (int)FDT_MAX_DEPTH) return -4;
            while (cursor < struct_size && structure[cursor] != 0U) ++cursor;
            if (cursor >= struct_size) return -5;
            ++cursor;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -6;
            ++depth;
            current_parent[depth] = depth == 0 ? 0U : current_parent[depth - 1];
            node_phandle[depth] = 0U;
            interrupt_cells[depth] = 0U;
            node_is_interrupt_controller[depth] = 0;
            node_compatible[depth] = NULL;
            node_compatible_length[depth] = 0U;
            node_interrupts[depth] = NULL;
            node_interrupts_length[depth] = 0U;
            node_status[depth] = NULL;
            node_status_length[depth] = 0U;
            node_has_reg[depth] = 0;
        } else if (token == FDT_END_NODE) {
            if (depth < 0) return -7;
            if (node_compatible[depth] != NULL && node_has_reg[depth] &&
                compatible_list_contains(node_compatible[depth],
                                         node_compatible_length[depth],
                                         compatible)) {
                int enabled = fdt_node_status_enabled(node_status[depth],
                                                       node_status_length[depth]);
                if (enabled < 0) return -16;
                if (enabled == 1 && matched_targets++ == instance_index) {
                    target_parent = current_parent[depth];
                    target_interrupts = node_interrupts[depth];
                    target_interrupts_length = node_interrupts_length[depth];
                    target_found = 1;
                }
            }
            if (node_is_interrupt_controller[depth] && node_phandle[depth] != 0U &&
                interrupt_cells[depth] == 3U && node_compatible[depth] != NULL &&
                (compatible_list_contains(node_compatible[depth],
                                          node_compatible_length[depth],
                                          "arm,gic-400") ||
                 compatible_list_contains(node_compatible[depth],
                                          node_compatible_length[depth],
                                          "arm,cortex-a15-gic")) &&
                controller_count < FDT_MAX_GIC_CONTROLLERS) {
                /* A nonzero #interrupt-cells plus phandle identifies a
                 * candidate interrupt parent; the binding is checked by
                 * the exact three-cell GICv2 decoder below. */
                controller_phandles[controller_count] = node_phandle[depth];
                controller_cells[controller_count] = interrupt_cells[depth];
                ++controller_count;
            }
            --depth;
        } else if (token == FDT_PROP) {
            uint32_t length, name_offset;
            const uint8_t *data;
            if (depth < 0 || cursor + 8U > struct_size) return -8;
            length = read_be32(structure + cursor);
            name_offset = read_be32(structure + cursor + 4U);
            cursor += 8U;
            if (length > struct_size - cursor || name_offset >= strings_size)
                return -9;
            data = structure + cursor;
            if (fdt_string_equals(strings, strings_size, name_offset, "compatible")) {
                node_compatible[depth] = data;
                node_compatible_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "interrupts")) {
                node_interrupts[depth] = data;
                node_interrupts_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "reg")) {
                node_has_reg[depth] = 1;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "status")) {
                node_status[depth] = data;
                node_status_length[depth] = length;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "interrupt-parent") && length == 4U) {
                current_parent[depth] = read_be32(data);
            } else if ((fdt_string_equals(strings, strings_size, name_offset, "phandle") ||
                        fdt_string_equals(strings, strings_size, name_offset, "linux,phandle")) &&
                       length == 4U) {
                node_phandle[depth] = read_be32(data);
            } else if (fdt_string_equals(strings, strings_size, name_offset, "#interrupt-cells") &&
                       length == 4U) {
                interrupt_cells[depth] = read_be32(data);
            } else if (fdt_string_equals(strings, strings_size, name_offset,
                                         "interrupt-controller") && length == 0U) {
                node_is_interrupt_controller[depth] = 1;
            }
            cursor += length;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -10;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            if (!target_found || target_interrupts == NULL) return 0; /* No matching node or IRQ property. */
            if (target_parent == 0U || target_interrupts_length < 12U ||
                (target_interrupts_length % 12U) != 0U) return -13;
            for (i = 0U; i < controller_count; ++i) {
                if (controller_phandles[i] == target_parent &&
                    controller_cells[i] == 3U) {
                    return nova_fdt_decode_gicv2_interrupt(
                        read_be32(target_interrupts),
                        read_be32(target_interrupts + 4U),
                        read_be32(target_interrupts + 8U), interrupt_id) ? 1 : -14;
                }
            }
            return -15; /* Interrupt parent is unsupported or not present. */
        } else {
            return -11;
        }
    }
    return -12;
}


int nova_fdt_find_gicv2_irq(uintptr_t dtb_address, const char *compatible,
                            uint32_t *interrupt_id) {
    return fdt_find_gicv2_irq_instance(dtb_address, compatible, 0U,
                                       interrupt_id);
}

int nova_fdt_find_gicv2_irq_instance(uintptr_t dtb_address,
                                     const char *compatible,
                                     size_t instance_index,
                                     uint32_t *interrupt_id) {
    return fdt_find_gicv2_irq_instance(dtb_address, compatible,
                                       instance_index, interrupt_id);
}

