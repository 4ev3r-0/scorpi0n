ENTRY(_start)

MEMORY
{
    IRAM (rx)  : ORIGIN = 0x40370000, LENGTH = 0x80000
    DRAM (rw)  : ORIGIN = 0x3FC88000, LENGTH = 0x50000
}

SECTIONS
{
    .text :
    {
        . = ALIGN(16);
        *(.text)
        *(.text.*)
        . = ALIGN(16);
    } > IRAM

    .rodata :
    {
        . = ALIGN(16);
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(16);
    } > IRAM

    .data :
    {
        . = ALIGN(16);
        *(.data)
        *(.data.*)
        . = ALIGN(16);
    } > DRAM

    .bss :
    {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
        . = ALIGN(16);
    } > DRAM

    . = ALIGN(16);

    _stack_top = ORIGIN(DRAM) + LENGTH(DRAM) - 16;
}
