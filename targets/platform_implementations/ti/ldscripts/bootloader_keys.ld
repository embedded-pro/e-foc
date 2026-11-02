/* The bootloader's key slot (documentation/design/bootloader.md, Part G): the last 80 bytes of its region, written
   after the link, so the code and data above it are the same with or without keys */
SECTIONS
{
    .bootloader_keys ORIGIN(REGION_TEXT) + LENGTH(REGION_TEXT) - 80 :
    {
        KEEP(*(.bootloader_keys))
    } > REGION_TEXT
}

ASSERT(SIZEOF(.bootloader_keys) == 80, "the bootloader key slot is not 80 bytes")
