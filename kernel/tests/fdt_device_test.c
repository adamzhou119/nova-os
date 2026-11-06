#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fdt.h"
#include "device.h"

static uint8_t blob[2048];
static uint32_t structure_cursor;
static uint32_t memory_reg_data_offset;
static uint32_t soc_ranges_data_offset;
static uint32_t soc_status_data_offset;

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

static uint8_t *find_first_status_value(uint32_t *length_out) {
    uint32_t cursor = 56U;
    uint32_t end = (((uint32_t)blob[36U] << 24) |
                    ((uint32_t)blob[37U] << 16) |
                    ((uint32_t)blob[38U] << 8) |
                    (uint32_t)blob[39U]) + 56U;
    while (cursor + 4U <= end) {
        uint32_t token = ((uint32_t)blob[cursor] << 24) |
                         ((uint32_t)blob[cursor + 1U] << 16) |
                         ((uint32_t)blob[cursor + 2U] << 8) |
                         (uint32_t)blob[cursor + 3U];
        cursor += 4U;
        if (token == 1U) {
            while (cursor < end && blob[cursor] != 0U) ++cursor;
            if (cursor >= end) return NULL;
            ++cursor;
            cursor = (cursor + 3U) & ~3U;
        } else if (token == 3U) {
            uint32_t length = ((uint32_t)blob[cursor] << 24) |
                              ((uint32_t)blob[cursor + 1U] << 16) |
                              ((uint32_t)blob[cursor + 2U] << 8) |
                              (uint32_t)blob[cursor + 3U];
            uint32_t name_offset = ((uint32_t)blob[cursor + 4U] << 24) |
                                  ((uint32_t)blob[cursor + 5U] << 16) |
                                  ((uint32_t)blob[cursor + 6U] << 8) |
                                  (uint32_t)blob[cursor + 7U];
            uint32_t data_offset = cursor + 8U;
            if (length > end - data_offset) return NULL;
            if (name_offset == 123U) {
                *length_out = length;
                return blob + data_offset;
            }
            cursor = data_offset + length;
            cursor = (cursor + 3U) & ~3U;
        } else if (token == 2U || token == 4U) {
            continue;
        } else {
            break;
        }
    }
    return NULL;
}

static void build_test_dtb(void) {
    static const char strings[] =
        "compatible\0#address-cells\0#size-cells\0reg\0ranges\0interrupt-parent\0interrupts\0phandle\0#interrupt-cells\0interrupt-controller\0status\0";
    static const char compatibles[] = "vendor,uart\0arm,pl011\0";
    static const char gic_compatible[] = "arm,gic-400";
    uint8_t cells[32];
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
    cells[16] = 0x00U; cells[17] = 0x00U; cells[18] = 0x00U; cells[19] = 0x00U;
    cells[20] = 0x0aU; cells[21] = 0x00U; cells[22] = 0x00U; cells[23] = 0x00U;
    cells[24] = 0x00U; cells[25] = 0x00U; cells[26] = 0x00U; cells[27] = 0x00U;
    cells[28] = 0x00U; cells[29] = 0x00U; cells[30] = 0x20U; cells[31] = 0x00U;
    append_property(38U, cells, sizeof(cells));
    {
        static const char enabled_status[] = "okay";
        append_property(123U, enabled_status, sizeof(enabled_status));
    }
    append_be32(2U);
    append_node("uart@b000000");
    {
        static const char second_uart[] = "vendor,uart\0arm,pl011";
        uint8_t second_reg[16] = {
            0U, 0U, 0U, 0U, 0x0bU, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0U, 0U, 0x10U, 0U
        };
        uint8_t parent[4] = {0U, 0U, 0U, 1U};
        uint8_t irq[12] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 5U,
                           0U, 0U, 0U, 4U};
        append_property(0U, second_uart, sizeof(second_uart));
        append_property(38U, second_reg, sizeof(second_reg));
        append_property(49U, parent, sizeof(parent));
        append_property(66U, irq, sizeof(irq));
        {
            static const char enabled_status[] = "ok";
            append_property(123U, enabled_status, sizeof(enabled_status));
        }
    }
    append_be32(2U);
    append_node("uart@c000000");
    {
        static const char disabled_uart[] = "vendor,uart\0arm,pl011";
        static const char disabled_status[] = "disabled";
        uint8_t disabled_reg[16] = {
            0U, 0U, 0U, 0U, 0x0cU, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0U, 0U, 0x10U, 0U
        };
        append_property(0U, disabled_uart, sizeof(disabled_uart));
        append_property(38U, disabled_reg, sizeof(disabled_reg));
        append_property(123U, disabled_status, sizeof(disabled_status));
    }
    append_be32(2U);
    append_node("soc");
    {
        uint8_t one[4] = {0U, 0U, 0U, 1U};
        uint8_t ranges[16] = {
            0U, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0x10U, 0U, 0U, 0U,
            0x01U, 0U, 0U, 0U
        };
        uint8_t parent[4] = {0U, 0U, 0U, 1U};
        static const char parent_status[] = "ok";
        append_property(11U, one, sizeof(one));
        append_property(26U, one, sizeof(one));
        soc_ranges_data_offset = structure_cursor + 12U;
        append_property(42U, ranges, sizeof(ranges));
        append_property(49U, parent, sizeof(parent));
        soc_status_data_offset = structure_cursor + 12U;
        append_property(123U, parent_status, sizeof(parent_status));
    }
    append_node("device@900000");
    {
        static const char nested_compatible[] = "vendor,nested";
        uint8_t nested_reg[8] = {0U, 0x90U, 0U, 0U, 0U, 0U, 0x10U, 0U};
        append_property(0U, nested_compatible, sizeof(nested_compatible));
        uint8_t irq[12] = {0U, 0U, 0U, 1U, 0U, 0U, 0U, 14U,
                           0U, 0U, 0U, 4U};
        append_property(38U, nested_reg, sizeof(nested_reg));
        append_property(66U, irq, sizeof(irq));
    }
    append_be32(2U);
    append_be32(2U);
    append_node("intc");
    {
        uint8_t phandle[4] = {0U, 0U, 0U, 1U};
        uint8_t cells[4] = {0U, 0U, 0U, 3U};
        static const uint8_t empty = 0U;
        static const uint8_t gic_regs[32] = {
            0U, 0U, 0U, 0U, 0x08U, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0U, 1U, 0U, 0U,
            0U, 0U, 0U, 0U, 0x08U, 1U, 0U, 0U,
            0U, 0U, 0U, 0U, 0U, 0U, 0x20U, 0U
        };
        append_property(0U, gic_compatible, sizeof(gic_compatible));
        append_property(38U, gic_regs, sizeof(gic_regs));
        append_property(77U, phandle, sizeof(phandle));
        append_property(85U, cells, sizeof(cells));
        append_property(102U, &empty, 0U);
    }
    append_be32(2U); /* Close the intc node. */
    append_node("memory@40000000");
    {
        /* Root bus widths are 2/2, so this reg is four cells wide.
         * The memory node's own 1/1 properties apply only to its children. */
        uint8_t memory_reg[16] = {
            0U, 0U, 0U, 0U, 0x40U, 0U, 0U, 0U,
            0U, 0U, 0U, 0U, 0x40U, 0U, 0U, 0U
        };
        uint8_t one[4] = {0U, 0U, 0U, 1U};
        append_property(11U, one, sizeof(one));
        append_property(26U, one, sizeof(one));
        memory_reg_data_offset = structure_cursor + 12U;
        append_property(38U, memory_reg, sizeof(memory_reg));
    }
    append_be32(2U); /* Close the memory node. */
    /* An unrelated PCI host bridge legitimately uses three address cells.
     * Its width must not abort discovery of the root-bus PL011 device.
     */
    append_node("pcie@10000000");
    {
        uint8_t three[4] = {0U, 0U, 0U, 3U};
        uint8_t two[4] = {0U, 0U, 0U, 2U};
        static const char pci_compatible[] = "pci-host-ecam-generic";
        append_property(0U, pci_compatible, sizeof(pci_compatible));
        append_property(11U, three, sizeof(three));
        append_property(26U, two, sizeof(two));
    }
    append_be32(2U); /* Close the PCI host bridge. */
    append_be32(2U); /* Close the root node. */
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
    struct nova_memory_region memory[8];
    build_test_dtb();
    assert(nova_fdt_memory_regions((uintptr_t)blob, memory, 8U) == 1);
    assert(memory[0].start == 0x40000000ULL);
    assert(memory[0].size == 0x40000000ULL);
    {
        uint8_t saved_size[8];
        memcpy(saved_size, blob + memory_reg_data_offset + 8U,
               sizeof(saved_size));
        /* A wrapping physical range is malformed, not silently discarded. */
        memset(blob + memory_reg_data_offset + 8U, 0xff, sizeof(saved_size));
        assert(nova_fdt_memory_regions((uintptr_t)blob, memory, 8U) < 0);
        memcpy(blob + memory_reg_data_offset + 8U, saved_size,
               sizeof(saved_size));
    }
    {
        const uint8_t valid_size[8] =
            {0U, 0U, 0U, 0U, 0x40U, 0U, 0U, 0U};
        memset(blob + memory_reg_data_offset + 8U, 0, sizeof(valid_size));
        /* A memory reg tuple with no usable bytes must be rejected. */
        assert(nova_fdt_memory_regions((uintptr_t)blob, memory, 8U) < 0);
        memcpy(blob + memory_reg_data_offset + 8U, valid_size,
               sizeof(valid_size));
    }
    {
        uint8_t saved_root_begin[4];
        struct nova_memory_region regions[8];
        memcpy(saved_root_begin, blob + 56U, sizeof(saved_root_begin));
        /* An FDT_END token cannot substitute for the mandatory root node. */
        put_be32(56U, 9U);
        assert(nova_fdt_memory_regions((uintptr_t)blob, regions, 8U) < 0);
        assert(nova_fdt_reserved_regions((uintptr_t)blob, regions, 8U) < 0);
        memcpy(blob + 56U, saved_root_begin, sizeof(saved_root_begin));
    }
    {
        struct nova_memory_region reserved[8];
        uint32_t original_reserve_offset =
            ((uint32_t)blob[16U] << 24) |
            ((uint32_t)blob[17U] << 16) |
            ((uint32_t)blob[18U] << 8) |
            (uint32_t)blob[19U];
        /* The parser relies on the v17 size_dt_struct header field. */
        {
            uint32_t irq = UINT32_MAX;
            uint32_t saved_version =
                ((uint32_t)blob[20U] << 24) |
                ((uint32_t)blob[21U] << 16) |
                ((uint32_t)blob[22U] << 8) | (uint32_t)blob[23U];
            uint32_t saved_compat =
                ((uint32_t)blob[24U] << 24) |
                ((uint32_t)blob[25U] << 16) |
                ((uint32_t)blob[26U] << 8) | (uint32_t)blob[27U];
            put_be32(20U, 16U);
            assert(nova_fdt_total_size((uintptr_t)blob) == 0U);
            assert(nova_fdt_memory_regions((uintptr_t)blob, memory, 8U) < 0);
            assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) < 0);
            assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                        &device) < 0);
            assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "arm,pl011",
                                           &irq) < 0);
            put_be32(20U, saved_version);
            put_be32(24U, 18U);
            assert(nova_fdt_total_size((uintptr_t)blob) == 0U);
            assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) < 0);
            put_be32(24U, saved_compat);
        }
        /* The fixture has an empty, aligned reservation map before structure. */
        assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) == 0);
        put_be32(16U, 44U); /* Reservation map must be 8-byte aligned. */
        assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) < 0);
        put_be32(16U, 32U); /* It must not overlap the header. */
        assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) < 0);
        put_be32(16U, 56U); /* It must not overlap the structure block. */
        assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) < 0);
        put_be32(16U, original_reserve_offset);
        /* A non-terminating entry cannot run into the structure block. */
        put_be32(original_reserve_offset, 1U);
        assert(nova_fdt_reserved_regions((uintptr_t)blob, reserved, 8U) < 0);
        put_be32(original_reserve_offset, 0U);
    }
    assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011", &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x09000000U);
    assert(device.mmio_size == 0x1000U);
    assert(nova_fdt_find_device_reg((uintptr_t)blob, "arm,pl011", 1U, &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x0a000000U);
    assert(device.mmio_size == 0x2000U);
    assert(nova_fdt_find_device_reg((uintptr_t)blob, "arm,pl011", 2U, &device) == 0);
    assert(nova_fdt_find_device_instance((uintptr_t)blob, "arm,pl011",
                                         1U, 0U, &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x0b000000U);
    assert(device.mmio_size == 0x1000U);
    assert(nova_fdt_find_device_instance((uintptr_t)blob, "arm,pl011",
                                         2U, 0U, &device) == 0);
    {
        uint32_t irq = UINT32_MAX;
        assert(nova_fdt_find_gicv2_irq_instance((uintptr_t)blob, "arm,pl011",
                                                1U, &irq) == 1);
        assert(irq == 37U);
        assert(nova_fdt_find_gicv2_irq_instance((uintptr_t)blob, "arm,pl011",
                                                2U, &irq) == 0);
    }
    assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x10900000U);
    assert(device.mmio_size == 0x1000U);
    {
        uint32_t irq = UINT32_MAX;
        /* A disabled parent bus makes its otherwise-enabled child unavailable. */
        blob[soc_status_data_offset] = (uint8_t)'n';
        blob[soc_status_data_offset + 1U] = (uint8_t)'o';
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) == 0);
        assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "vendor,nested", &irq) == 0);
        blob[soc_status_data_offset] = (uint8_t)'o';
        blob[soc_status_data_offset + 1U] = (uint8_t)'k';
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) == 1);
        assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "vendor,nested", &irq) == 1);
    }
    {
        uint8_t saved_blob[sizeof(blob)];
        uint32_t total_size, structure_size, strings_offset;
        memcpy(saved_blob, blob, sizeof(blob));

        /* A missing ranges property must not silently become identity mapping. */
        put_be32(soc_ranges_data_offset - 4U, 0U);
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) < 0);
        memcpy(blob, saved_blob, sizeof(blob));

        /* A present but empty ranges property explicitly means identity mapping. */
        total_size = ((uint32_t)blob[4U] << 24) | ((uint32_t)blob[5U] << 16) |
                     ((uint32_t)blob[6U] << 8) | blob[7U];
        structure_size = ((uint32_t)blob[36U] << 24) | ((uint32_t)blob[37U] << 16) |
                         ((uint32_t)blob[38U] << 8) | blob[39U];
        strings_offset = ((uint32_t)blob[12U] << 24) | ((uint32_t)blob[13U] << 16) |
                         ((uint32_t)blob[14U] << 8) | blob[15U];
        put_be32(soc_ranges_data_offset - 8U, 0U);
        memmove(blob + soc_ranges_data_offset,
                blob + soc_ranges_data_offset + 16U,
                total_size - soc_ranges_data_offset - 16U);
        put_be32(4U, total_size - 16U);
        put_be32(12U, strings_offset - 16U);
        put_be32(36U, structure_size - 16U);
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) == 1);
        assert(device.mmio_base == (uintptr_t)0x00900000U);
        assert(device.mmio_size == 0x1000U);
        memcpy(blob, saved_blob, sizeof(blob));

        /* Reject a ranges tuple whose span does not cover the complete device. */
        put_be32(soc_ranges_data_offset + 12U, 1U);
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) < 0);
        memcpy(blob, saved_blob, sizeof(blob));

        /* Reject malformed tuple lengths rather than reading partial tuples. */
        put_be32(soc_ranges_data_offset - 8U, 15U);
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) < 0);
        memcpy(blob, saved_blob, sizeof(blob));

        /* Reject parent-window arithmetic that wraps the 64-bit address space. */
        put_be32(soc_ranges_data_offset + 4U, UINT32_MAX);
        put_be32(soc_ranges_data_offset + 8U, UINT32_MAX);
        assert(nova_fdt_find_device((uintptr_t)blob, "vendor,nested", &device) < 0);
        memcpy(blob, saved_blob, sizeof(blob));
    }
    assert(nova_fdt_find_device_reg((uintptr_t)blob, "arm,gic-400", 0U,
                                    &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x08000000U);
    assert(device.mmio_size == 0x10000U);
    assert(nova_fdt_find_device_reg((uintptr_t)blob, "arm,gic-400", 1U,
                                    &device) == 1);
    assert(device.mmio_base == (uintptr_t)0x08010000U);
    assert(device.mmio_size == 0x2000U);
    assert(nova_fdt_find_device_reg((uintptr_t)blob, "arm,gic-400", 2U,
                                    &device) == 0);
    assert(nova_fdt_find_device((uintptr_t)blob, "vendor,missing",
                                &device) == 0);
    {
        uint32_t strings_offset = ((uint32_t)blob[12U] << 24) |
                                  ((uint32_t)blob[13U] << 16) |
                                  ((uint32_t)blob[14U] << 8) |
                                  (uint32_t)blob[15U];
        uint32_t structure_offset = ((uint32_t)blob[8U] << 24) |
                                    ((uint32_t)blob[9U] << 16) |
                                    ((uint32_t)blob[10U] << 8) |
                                    (uint32_t)blob[11U];
        uint32_t irq = UINT32_MAX;
        struct nova_memory_region memory[8];
        put_be32(12U, structure_offset + 4U);
        assert(nova_fdt_memory_regions((uintptr_t)blob, memory, 8U) < 0);
        assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                    &device) < 0);
        assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "arm,pl011",
                                       &irq) < 0);
        put_be32(12U, strings_offset);
        put_be32(8U, structure_offset + 1U);
        assert(nova_fdt_memory_regions((uintptr_t)blob, memory, 8U) < 0);
        put_be32(8U, structure_offset);
    }
    {
        uint32_t end_offset = 56U +
            (((uint32_t)blob[36U] << 24) |
             ((uint32_t)blob[37U] << 16) |
             ((uint32_t)blob[38U] << 8) |
             (uint32_t)blob[39U]) - 4U;
        uint8_t saved_end[4];
        memcpy(saved_end, blob + end_offset, sizeof(saved_end));
        /* A valid early match must not hide a malformed trailing token. */
        memset(blob + end_offset, 0xff, sizeof(saved_end));
        assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                    &device) < 0);
        memcpy(blob + end_offset, saved_end, sizeof(saved_end));
        {
            uint8_t saved_root_end[4];
            struct nova_memory_region regions[8];
            uint32_t root_end_offset = end_offset - 4U;
            memcpy(saved_root_end, blob + root_end_offset,
                   sizeof(saved_root_end));
            /* FDT_END is invalid while the root node remains open. */
            put_be32(root_end_offset, 4U); /* FDT_NOP */
            assert(nova_fdt_memory_regions((uintptr_t)blob, regions, 8U) < 0);
            assert(nova_fdt_reserved_regions((uintptr_t)blob, regions, 8U) < 0);
            memcpy(blob + root_end_offset, saved_root_end,
                   sizeof(saved_root_end));
        }
    }
    {
        uint32_t irq = UINT32_MAX;
        assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "vendor,nested",
                                       &irq) == 1);
        assert(irq == 30U);
        assert(nova_device_discover_fdt((uintptr_t)blob, 7U,
                                        "vendor,nested") == 1);
        {
            struct nova_device_info info;
            assert(nova_device_get(7U, &info) == 1);
            assert(info.state == NOVA_DEVICE_DISCOVERED);
            assert(info.desc.mmio_base == (uintptr_t)0x10900000U);
            assert(info.desc.mmio_size == 0x1000U);
            assert(info.desc.irq == 30U);
            assert(strcmp(info.desc.compatible, "vendor,nested") == 0);
        }
        assert(nova_device_discover_fdt((uintptr_t)blob, 7U,
                                        "vendor,nested") == 0);
        assert(nova_device_discover_fdt_instance((uintptr_t)blob, 8U,
                                                  "arm,pl011", 1U) == 1);
        {
            size_t discovered = 99U;
            struct nova_device_info first, second;
            assert(nova_device_discover_fdt_all((uintptr_t)blob, 20U,
                                                "arm,pl011", &discovered) == 1);
            assert(discovered == 2U);
            assert(nova_fdt_find_device_instance((uintptr_t)blob, "arm,pl011",
                                                 2U, 0U, &device) == 0);
            assert(nova_device_get(20U, &first) == 1);
            assert(first.desc.mmio_base == (uintptr_t)0x09000000U);
            assert(first.desc.irq == NOVA_DEVICE_NO_IRQ);
            assert(nova_device_get(21U, &second) == 1);
            assert(second.desc.mmio_base == (uintptr_t)0x0b000000U);
            assert(second.desc.irq == 37U);

            /* A collision or ID overflow must leave the registry untouched. */
            discovered = 99U;
            assert(nova_device_discover_fdt_all((uintptr_t)blob, 7U,
                                                "arm,pl011", &discovered) == 0);
            assert(discovered == 0U);
            assert(nova_device_get(7U, &first) == 1);
            assert(nova_device_get(22U, &second) == 0);
            assert(nova_device_discover_fdt_all((uintptr_t)blob, UINT32_MAX,
                                                "arm,pl011", &discovered) == 0);
            assert(discovered == 0U);
            assert(nova_device_get(UINT32_MAX, &second) == 0);
        }
        {
            struct nova_device_info second;
            assert(nova_device_get(8U, &second) == 1);
            assert(second.desc.mmio_base == (uintptr_t)0x0b000000U);
            assert(second.desc.mmio_size == 0x1000U);
            assert(second.desc.irq == 37U);
            assert(strcmp(second.desc.compatible, "arm,pl011") == 0);
        }
        assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "vendor,missing",
                                       &irq) == 0);
    }
    assert(nova_fdt_find_device((uintptr_t)blob, "", &device) < 0);
    blob[0] = 0U;
    assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                &device) < 0);
    {
        uint32_t interrupt_id = 0U;
        assert(nova_fdt_decode_gicv2_interrupt(0U, 1U, 4U,
                                               &interrupt_id) == 1);
        assert(interrupt_id == 33U);
        assert(nova_fdt_decode_gicv2_interrupt(1U, 14U, 4U,
                                               &interrupt_id) == 1);
        assert(interrupt_id == 30U);
        assert(nova_fdt_decode_gicv2_interrupt(1U, 15U, 1U,
                                               &interrupt_id) == 1);
        assert(interrupt_id == 31U);
        assert(nova_fdt_decode_gicv2_interrupt(0U, 987U, 8U,
                                               &interrupt_id) == 1);
        assert(interrupt_id == 1019U);
        assert(nova_fdt_decode_gicv2_interrupt(0U, 988U, 4U,
                                               &interrupt_id) == 0);
        assert(nova_fdt_decode_gicv2_interrupt(1U, 16U, 4U,
                                               &interrupt_id) == 0);
        assert(nova_fdt_decode_gicv2_interrupt(2U, 0U, 4U,
                                               &interrupt_id) == 0);
        assert(nova_fdt_decode_gicv2_interrupt(0U, 1U, 0U,
                                               &interrupt_id) == 0);
        assert(nova_fdt_decode_gicv2_interrupt(0U, 1U, 4U, NULL) == 0);
    }
    {
        size_t discovered = 99U;
        struct nova_device_desc filler;
        uint32_t id;
        memset(&filler, 0, sizeof(filler));
        filler.irq = NOVA_DEVICE_NO_IRQ;
        strcpy(filler.compatible, "test,dummy");
        for (id = 100U; id < 112U; ++id) {
            filler.id = id;
            assert(nova_device_add(&filler) == 1);
        }
        assert(nova_device_discover_fdt_all((uintptr_t)blob, 50U,
                                            "arm,pl011", &discovered) == 0);
        assert(discovered == 0U);
        {
            struct nova_device_info info;
            assert(nova_device_get(50U, &info) == 0);
            assert(nova_device_get(51U, &info) == 0);
        }
    }
    {
        uint32_t status_length = 0U;
        uint8_t *status = find_first_status_value(&status_length);
        uint8_t saved_last;
        uint8_t saved_first;
        size_t discovered = 99U;
        struct nova_fdt_device malformed_device;
        struct nova_device_info info;
        assert(status != NULL && status_length == 5U);
        saved_last = status[status_length - 1U];
        saved_first = status[0];
        status[status_length - 1U] = (uint8_t)'X';
        assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                    &malformed_device) < 0);
        status[status_length - 1U] = saved_last;
        status[0] = 0U; /* Embedded/leading NUL is not a valid status string. */
        assert(nova_fdt_find_device((uintptr_t)blob, "arm,pl011",
                                    &malformed_device) < 0);
        status[0] = saved_first;
        status[status_length - 1U] = (uint8_t)'X';
        {
            uint32_t irq = UINT32_MAX;
            assert(nova_fdt_find_gicv2_irq((uintptr_t)blob, "arm,pl011",
                                           &irq) < 0);
        }
        assert(nova_device_discover_fdt_all((uintptr_t)blob, 60U,
                                            "arm,pl011", &discovered) == 0);
        assert(discovered == 0U);
        assert(nova_device_get(60U, &info) == 0);
        status[status_length - 1U] = saved_last;
    }
    puts("FDT device discovery and GICv2 decoding host tests passed.");
    return 0;
}
