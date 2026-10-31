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
    if (read_be32(base) != FDT_MAGIC) return -1;

    total_size = read_be32(base + 4U);
    struct_offset = read_be32(base + 8U);
    strings_offset = read_be32(base + 12U);
    strings_size = read_be32(base + 32U);
    struct_size = read_be32(base + 36U);

    if (total_size < FDT_HEADER_SIZE ||
        struct_offset > total_size || struct_size > total_size - struct_offset ||
        strings_offset > total_size || strings_size > total_size - strings_offset)
        return -1;

    structure = base + struct_offset;
    strings = base + strings_offset;

    while (cursor + 4U <= struct_size) {
        uint32_t token = read_be32(structure + cursor);
        cursor += 4U;

        if (token == FDT_BEGIN_NODE) {
            uint32_t name_start = cursor;
            uint32_t name_length;
            if (depth + 1 >= (int)FDT_MAX_DEPTH) return -1;
            while (cursor < struct_size && structure[cursor] != 0) ++cursor;
            if (cursor >= struct_size) return -1;
            name_length = cursor - name_start;
            ++cursor;
            cursor = (cursor + 3U) & ~3U;
            if (cursor > struct_size) return -1;

            ++depth;
            address_cells[depth] = depth == 0 ? 2U : address_cells[depth - 1];
            size_cells[depth] = depth == 0 ? 1U : size_cells[depth - 1];
            memory_node[depth] = node_is_memory((const char *)(structure + name_start));
            reg_data[depth] = NULL;
            reg_length[depth] = 0;
            (void)name_length;
        } else if (token == FDT_END_NODE) {
            if (depth < 0) return -1;
            if (memory_node[depth] && reg_data[depth] != NULL) {
                uint32_t tuple_cells = address_cells[depth] + size_cells[depth];
                uint32_t tuple_bytes;
                uint32_t pos = 0;
                if (tuple_cells == 0 || tuple_cells > 4U) return -1;
                tuple_bytes = tuple_cells * 4U;
                if (reg_length[depth] == 0 ||
                    reg_length[depth] % tuple_bytes != 0) return -1;

                while (pos < reg_length[depth]) {
                    uint64_t start;
                    uint64_t size;
                    if (!read_cells(reg_data[depth] + pos,
                                    address_cells[depth], &start) ||
                        !read_cells(reg_data[depth] + pos +
                                    address_cells[depth] * 4U,
                                    size_cells[depth], &size))
                        return -1;
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
            if (depth < 0 || cursor + 8U > struct_size) return -1;
            length = read_be32(structure + cursor);
            name_offset = read_be32(structure + cursor + 4U);
            cursor += 8U;
            if (length > struct_size - cursor || name_offset >= strings_size)
                return -1;
            data = structure + cursor;

            if (fdt_string_equals(strings, strings_size, name_offset, "#address-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells == 0 || cells > 2U) return -1;
                address_cells[depth] = cells;
            } else if (fdt_string_equals(strings, strings_size, name_offset, "#size-cells") && length == 4U) {
                uint32_t cells = read_be32(data);
                if (cells == 0 || cells > 2U) return -1;
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
            if (cursor > struct_size) return -1;
        } else if (token == FDT_NOP) {
            continue;
        } else if (token == FDT_END) {
            return found > capacity ? -1 : (int)found;
        } else {
            return -1;
        }
    }

    return -1;
}
