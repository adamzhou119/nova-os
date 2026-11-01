#include "gic.h"
#include "fdt.h"

#define GICD_DEFAULT_BASE 0x08000000UL
#define GICC_DEFAULT_BASE 0x08010000UL
static uintptr_t gicd_base = GICD_DEFAULT_BASE;
static uintptr_t gicc_base = GICC_DEFAULT_BASE;
#define GICD_CTLR (*(volatile uint32_t *)(gicd_base + 0x000U))
#define GICD_ISENABLER0 (*(volatile uint32_t *)(gicd_base + 0x100U))
#define GICC_CTLR (*(volatile uint32_t *)(gicc_base + 0x000U))
#define GICC_PMR (*(volatile uint32_t *)(gicc_base + 0x004U))
#define GICC_IAR (*(volatile uint32_t *)(gicc_base + 0x00cU))
#define GICC_EOIR (*(volatile uint32_t *)(gicc_base + 0x010U))
#define NOVA_TIMER_PPI 30U

int nova_gic_init(uintptr_t dtb_address) {
    struct nova_fdt_device distributor, cpu_interface;
    int found = nova_fdt_find_device_reg(dtb_address, "arm,cortex-a15-gic", 0U, &distributor);
    if (found != 1)
        found = nova_fdt_find_device_reg(dtb_address, "arm,gic-400", 0U, &distributor);
    if (found != 1 || distributor.mmio_size < 0x1000U) return 0;
    found = nova_fdt_find_device_reg(dtb_address, "arm,cortex-a15-gic", 1U, &cpu_interface);
    if (found != 1)
        found = nova_fdt_find_device_reg(dtb_address, "arm,gic-400", 1U, &cpu_interface);
    if (found != 1 || cpu_interface.mmio_size < 0x1000U) return 0;
    gicd_base = distributor.mmio_base;
    gicc_base = cpu_interface.mmio_base;
    /* Resolve GICv2 distributor and CPU-interface windows from FDT. */
    GICD_CTLR = 0U;
    GICC_CTLR = 0U;
    GICC_PMR = 0xffU;
    GICD_ISENABLER0 = (1U << NOVA_TIMER_PPI);
    GICC_CTLR = 1U;
    GICD_CTLR = 1U;
    __asm__ volatile("dsb sy\n\tisb" ::: "memory");
    return (GICD_CTLR & 1U) != 0U && (GICC_CTLR & 1U) != 0U;
}

uint32_t nova_gic_acknowledge(void) {
    uint32_t iar = GICC_IAR;
    return iar & 0x3ffU;
}

void nova_gic_end_interrupt(uint32_t interrupt_id) {
    GICC_EOIR = interrupt_id;
    __asm__ volatile("dsb sy" ::: "memory");
}
