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

    .heap (NOLOAD) : ALIGN(4096) {
        __heap_start = .;
        . += 0x100000;
        __heap_end = .;
    }

    /* Fixed QEMU prototype pool; replace with firmware memory discovery. */
    .page_pool (NOLOAD) : ALIGN(4096) {
        __page_pool_start = .;
        . += 0x400000;
        __page_pool_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
