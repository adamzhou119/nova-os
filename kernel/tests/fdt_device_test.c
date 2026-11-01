#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fdt.h"

static uint8_t blob[512];
static uint32_t structure_cursor;

static void put_be32(uint32_t offset, uint32_t value) {
    blob[offset] = (uint8_t)(value >> 24);
    blob[offset + 1U] = (uint8_t)(value >> 16);
    blob[offset + 2U] = (uint8_t)(value >> 8);
    blob[offset + 3U] = (uint8_t)value;
}

static void append_be32(uint32_t value) {
    put_be32(structure_cursor, value);
    structure_cursor += 4U;
}

static void append_bytes(const void *data, uint32_t length) {
    memcpy(blob + structure_cursor, data, length);
    structure_cursor += length;
    while ((structure_cursor & 3U) != 0U)
        blob[structure_cursor++] = 0U;
}

static void append_node(const char *name) {
    append_be32(1U);
    append_bytes(name, (uint32_t)strlen(name) + 1U);
}

static void append_property(uint32_t name_offset, const void *data,
                            uint32_t length) {
    append_be32(3U);
    append_be32(length);
    append_be32(name_offset);
    append_bytes(data, length);
}

static void build_test_dtb(void) {
    static const char strings[] =
        "compatible\0#address-cells\0#size-cells\0reg\0ranges\0";
    static const char compatibles[] = "vendor,uart\0arm,pl011\0";
    uint8_t cells[16];
    uint32_t structure_offset = 56U;

    memset(blob, 0, sizeof(blob));
    structure_cursor = structure_offset;
    append_node("");
    /* Root address/size cell widths are 2/2. */
    {
        uint8_t value[4] = {0U, 0U, 0U, 2U};
        append_property(11U, value, sizeof(value));
        append_property(26U, value, sizeof(value));
    }
    append_node("uart@9000000");
    append_property(0U, compatibles, sizeof(compatibles));
    memset(cells, 0, sizeof(cells));
    cells[4] = 0x09U;
    cells[5] = 0x00U;
    cells[6] = 0x00U;
    cells[7] = 0x00U;
    cells[12] = 0x00U;
    cells[13] = 0x00U;
    cells[14] = 0x10U;
    cells[15] = 0x00U;
    append_property(38U, cells, sizeof(cells));
    append_be32(2U);
    append_node("soc");
    {
        uint8_t one[4] = {0U, 0U, 0U, 1U};
        uint8_t ranges[16] = {
            0U, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0x10U, 0U, 0U, 0U,
            0x01U, 0U, 0U, 0U
        };
        append_property(11U, one, sizeof(one));
        append_property(26U, one, sizeof(one));
        append_property(42U, ranges, sizeof(ranges));
    }
    append_node("device@900000");
    {
        static const char nested_compatible[] = "vendor,nested";
        uint8_t nested_reg[8] = {0U, 0x90U, 0U, 0U, 0U, 0U, 0x10U, 0U};
        append_property(0U, nested_compatible, sizeof(nested_compatible));
        append_property(38U, nested_reg, sizeof(nested_reg));
    }
    append_be32(2U);
    append_be32(2U);
    append_be32(2U);
    append_be32(9U);

    memcpy(blob + structure_cursor, strings, sizeof(strings));
    put_be32(4U, structure_cursor + (uint32_t)sizeof(strings));
    put_be32(8U, structure_offset);
    put_be32(12U, structure_cursor);
    put_be32(16U, 40U);
    put_be32(20U, 17U);
    put_be32(24U, 16U);
    put_be32(28U, 0U);
    put_be32(32U, (uint32_t)sizeof(strings));
    put_be32(36U, structure_cursor - structure_offset);
    blob[0] = 0xd0U; blob[1] = 0x0dU;
    blob[2] = 0xfeU; blob[3] = 0xedU;
    put_be32(4U, structure_cursor + (uint32_t)sizeof(strings));
}

int main(void) {
    struct nova_fdt_device device = {0U, 0U};
    build_test_dtb();
    assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011", &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x09000000U);
    assert(device.mmio_size == 0x1000U);
    assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x10900000U);
    assert(device.mmio_size == 0x1000U);
    assert(nova_fdt_find_device((uintptr_t)blob, "vendor,missing",
                                &device) == 0);
    assert(nova_fdt_find_device((uintptr_t)blob, "", &device) < 0);
    blob[0] = 0U;
    assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                &device) < 0);
    puts("FDT device discovery host tests passed.");
    return 0;
}
