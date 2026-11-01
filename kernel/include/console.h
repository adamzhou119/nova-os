#ifndef NOVA_CONSOLE_H
#define NOVA_CONSOLE_H

#include <stdint.h>

/* Early boot console API; backend is currently the QEMU virt PL011 UART. */
#define NOVA_CONSOLE_UART_PHYS_BASE 0x09000000UL
int nova_console_init(uintptr_t dtb_address);
void nova_console_putc(char value);
void nova_console_write(const char *text);
void nova_console_write_hex64(uint64_t value);
void nova_console_write_current_el(uint64_t current_el);

#endif
