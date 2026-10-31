#ifndef NOVA_EXCEPTION_H
#define NOVA_EXCEPTION_H

#include <stdint.h>

/* Architectural vector-slot class, independent of the exception syndrome. */
const char *nova_exception_kind(uint64_t vector_slot);

/* Human-readable ESR_EL1.EC decoding; unknown values remain explicit. */
const char *nova_exception_class_name(uint64_t esr_el1);

#endif
