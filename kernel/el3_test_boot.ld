ENTRY(_el3_test_start)
SECTIONS
{
    /* QEMU virt RAM begins at 0x40000000; address zero is not RAM. */
    . = 0x40000000;
    .text : { KEEP(*(.text.boot)) *(.text*) }
}
