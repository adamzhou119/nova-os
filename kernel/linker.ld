ENTRY(_start)

SECTIONS
{
    . = 0x40080000;
    __kernel_start = .;

    .text : ALIGN(4096) {
        __text_start = .;
        KEEP(*(.text.boot))
        *(.text .text.*)
        . = ALIGN(4096);
        __text_end = .;
    }

    .rodata : ALIGN(4096) {
        __rodata_start = .;
        *(.rodata .rodata.*)
        . = ALIGN(4096);
        __rodata_end = .;
    }

    .data : ALIGN(4096) {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4096);
        __data_end = .;
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

    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
