#ifndef NOVA_INTERRUPT_H
#define NOVA_INTERRUPT_H
#include <stdint.h>

/* Layout shared with exceptions.S; frame size is 272 bytes. */
struct nova_irq_frame {
    uint64_t x[31];
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t sp_el0;
};
_Static_assert(sizeof(struct nova_irq_frame) == 272U, "IRQ frame size mismatch");
_Static_assert(__builtin_offsetof(struct nova_irq_frame, elr_el1) == 248U,
               "IRQ ELR offset mismatch");
_Static_assert(__builtin_offsetof(struct nova_irq_frame, spsr_el1) == 256U,
               "IRQ SPSR offset mismatch");
_Static_assert(__builtin_offsetof(struct nova_irq_frame, sp_el0) == 264U,
               "IRQ SP_EL0 offset mismatch");

/* Returns the frame to restore; currently returns the interrupted frame. */
uintptr_t nova_irq_dispatch(uintptr_t frame_sp);
uint64_t nova_irq_tick_count(void);
#endif
