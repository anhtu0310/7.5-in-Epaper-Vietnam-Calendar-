/* nRF51822-QFAA / xxAA: 256 KiB flash, 16 KiB RAM. */
ENTRY(Reset_Handler)

MEMORY
{
    /* Last two 1 KiB pages are the date journal. */
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x0003F800
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00004000
}

INCLUDE linker/sections.ld
