__Main_Stack_Size = 16K;

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 32K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 4M
}

ASSERT(ORIGIN(FLASH) == __bootloader_start && ORIGIN(FLASH) + LENGTH(FLASH) == __bootloader_end, "the bootloader does not fill its region")
ASSERT(ORIGIN(RAM) == __ram_start && ORIGIN(RAM) + LENGTH(RAM) == __ram_end, "RAM differs from the layout")
