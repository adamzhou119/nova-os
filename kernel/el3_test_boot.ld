ENTRY(_el3_test_start)
SECTIONS
{
    . = 0x00000000;
    .text : { KEEP(*(.text.boot)) *(.text*) }
}
