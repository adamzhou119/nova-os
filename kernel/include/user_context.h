#ifndef NOVA_USER_CONTEXT_H
#define NOVA_USER_CONTEXT_H

#include <stdint.h>

/*
 * EL0 resumable trap frame. Keep this layout synchronized with the
 * lower-EL exception entry/return assembly before enabling user execution.
 * This type is deliberately separate from the legacy 272-byte EL1 IRQ frame.
 */
struct nova_user_trap_frame {
    uint64_t x[31];
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t sp_el0;
    uint64_t esr_el1;
    uint64_t far_el1;
};

_Static_assert(sizeof(struct nova_user_trap_frame) == 288U,
               "user trap frame size mismatch");
_Static_assert(__builtin_offsetof(struct nova_user_trap_frame, elr_el1) == 248U,
               "user trap ELR offset mismatch");
_Static_assert(__builtin_offsetof(struct nova_user_trap_frame, spsr_el1) == 256U,
               "user trap SPSR offset mismatch");
_Static_assert(__builtin_offsetof(struct nova_user_trap_frame, sp_el0) == 264U,
               "user trap SP_EL0 offset mismatch");
_Static_assert(__builtin_offsetof(struct nova_user_trap_frame, esr_el1) == 272U,
               "user trap ESR offset mismatch");
_Static_assert(__builtin_offsetof(struct nova_user_trap_frame, far_el1) == 280U,
               "user trap FAR offset mismatch");

#endif