#include "timer.h"

#define NOVA_TIMER_CTL_ENABLE (1U << 0)
#define NOVA_TIMER_CTL_IMASK  (1U << 1)
#define NOVA_TIMER_CTL_STATUS (1U << 2)
#define NOVA_TIMER_MAX_DELTA  0x7fffffffULL

static uint64_t timer_frequency;

static uint64_t read_frequency(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

int nova_timer_init(void) {
    timer_frequency = read_frequency();
    nova_timer_stop();
    return timer_frequency != 0U;
}

uint64_t nova_timer_frequency(void) {
    return timer_frequency;
}

uint64_t nova_timer_now(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
}

int nova_timer_arm(uint64_t delta_ticks) {
    uint64_t control = NOVA_TIMER_CTL_IMASK;
    if (timer_frequency == 0U || delta_ticks == 0U ||
        delta_ticks > NOVA_TIMER_MAX_DELTA) return 0;
    __asm__ volatile("msr cntp_ctl_el0, %0\n\tisb" :: "r"(control) : "memory");
    __asm__ volatile("msr cntp_tval_el0, %0\n\tisb"
                     :: "r"(delta_ticks) : "memory");
    control = NOVA_TIMER_CTL_ENABLE | NOVA_TIMER_CTL_IMASK;
    __asm__ volatile("msr cntp_ctl_el0, %0\n\tisb" :: "r"(control) : "memory");
    return 1;
}

int nova_timer_expired(void) {
    uint64_t control;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(control));
    return (control & NOVA_TIMER_CTL_STATUS) != 0U;
}

void nova_timer_stop(void) {
    uint64_t control = NOVA_TIMER_CTL_IMASK;
    __asm__ volatile("msr cntp_ctl_el0, %0\n\tisb" :: "r"(control) : "memory");
}
