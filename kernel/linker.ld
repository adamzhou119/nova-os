ENTRY(_start)

SECTIONS
{
    . = 0x40080000;

    .text : ALIGN(4096) {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss.stack)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    }

    /* A 1 MiB bootstrap heap in QEMU RAM; not a physical RAM discovery map. */
    .heap (NOLOAD) : ALIGN(4096) {
        __heap_start = .;
        . += 0x100000;
        __heap_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
