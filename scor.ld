OUTPUT_ARCH("xtensa")
ENTRY(scor_main)

MEMORY
{
    psram (rwx) : ORIGIN = 0x3C100000, LENGTH = 4M
}

SECTIONS
{
    .text : {
        *(.text.entry)
        *(.text*)
        *(.gnu.linkonce.t*)
    } > psram

    .rodata : {
        *(.rodata*)
        *(.gnu.linkonce.r*)
    } > psram

    .data : {
        *(.data*)
        *(.gnu.linkonce.d*)
    } > psram

    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss*)
        *(.gnu.linkonce.b*)
        __bss_end__ = .;
    } > psram

    /* Vtable at a FIXED offset after .bss — loader writes here */
    .scor_api (NOLOAD) : {
        scor_api = .;
        . += 48;  /* sizeof(scor_api_t) = 9 pointers × 4 bytes = 36, pad to 48 */
    } > psram
}   