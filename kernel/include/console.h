#ifndef NOVA_CONSOLE_H
#define NOVA_CONSOLE_H

#include <stdint.h>

/* Early boot console API; backend is currently the QEMU virt PL011 UART. */
void nova_console_putc(char value);
void nova_console_write(const char *text);
void nova_console_write_hex64(uint64_t value);
void nova_console_write_current_el(uint64_t current_el);

#endif
