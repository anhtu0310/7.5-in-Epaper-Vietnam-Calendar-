/* nRF51822-QFAA with S130 v2.0.1. Last two pages hold the date journal. */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x0001B000, LENGTH = 0x00024800
    /* Exact value returned by sd_ble_enable() for this BLE configuration. */
    RAM   (rwx) : ORIGIN = 0x20002078, LENGTH = 0x00001F88
}

INCLUDE linker/sections.ld
