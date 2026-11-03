MEMORY
{
    FLASH (rx)     : ORIGIN = 0x00008000, LENGTH = 256K - 64
    DESCRIPTOR (r) : ORIGIN = 0x00047FC0, LENGTH = 64
    RAM (rwx)      : ORIGIN = 0x20000000, LENGTH = 256K
}

ASSERT(ORIGIN(FLASH) == __application_slot_start, "the application does not start at its slot")
ASSERT(ORIGIN(DESCRIPTOR) + LENGTH(DESCRIPTOR) == __application_slot_end, "the image descriptor does not end the slot")
ASSERT(ORIGIN(RAM) == __ram_start && ORIGIN(RAM) + LENGTH(RAM) == __ram_end, "RAM differs from the flash layout")
