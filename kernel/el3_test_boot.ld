ENTRY(_el3_test_start)
SECTIONS
{
    /* Keep the firmware after QEMU's secure-virt DTB reservation. */
    . = 0x40100000;
    .text : { KEEP(*(.text.boot)) *(.text*) }
}
