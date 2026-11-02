#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fdt.h"
#include "device.h"

static uint8_t blob[1024];
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
        append_property(11U, one, sizeof(one));
        append_property(26U, one, sizeof(one));
        append_property(42U, ranges, sizeof(ranges));
        append_property(49U, parent, sizeof(parent));
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
        assert(nova_device_discover_fdt_all((uintptr_t)blob, 200U,
                                            "vendor,missing", &discovered) == 1);
        assert(discovered == 0U);
    }
    puts("FDT device discovery and GICv2 decoding host tests passed.");
    return 0;
}
