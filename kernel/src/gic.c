#include "gic.h"

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL
#define GICD_CTLR (*(volatile uint32_t *)(GICD_BASE + 0x000U))
#define GICD_ISENABLER0 (*(volatile uint32_t *)(GICD_BASE + 0x100U))
#define GICC_CTLR (*(volatile uint32_t *)(GICC_BASE + 0x000U))
#define GICC_PMR (*(volatile uint32_t *)(GICC_BASE + 0x004U))
#define GICC_IAR (*(volatile uint32_t *)(GICC_BASE + 0x00cU))
#define GICC_EOIR (*(volatile uint32_t *)(GICC_BASE + 0x010U))
#define NOVA_TIMER_PPI 30U

int nova_gic_init(void) {
    /* QEMU virt with gic-version=2: enable distributor, CPU interface and PPI 30. */
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
