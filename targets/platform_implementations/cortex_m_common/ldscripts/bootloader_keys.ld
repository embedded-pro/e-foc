SECTIONS
{
    .bootloader_keys ORIGIN(REGION_TEXT) + LENGTH(REGION_TEXT) - 80 :
    {
        KEEP(*(.bootloader_keys))
    } > REGION_TEXT
}

ASSERT(SIZEOF(.bootloader_keys) == 80, "the bootloader key slot is not 80 bytes")
