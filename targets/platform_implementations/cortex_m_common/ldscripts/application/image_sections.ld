.application_header ORIGIN(REGION_TEXT) + 0x400 :
{
    KEEP(*(.application_header))
} > REGION_TEXT

.image_descriptor :
{
    KEEP(*(.image_descriptor))
} > DESCRIPTOR

ASSERT(SIZEOF(.isr_vector) <= 0x400, "the vector table reaches into the application header at 1 KiB")
ASSERT(SIZEOF(.application_header) == 64, "the application header is not 64 bytes")
ASSERT(SIZEOF(.image_descriptor) == 64, "the image descriptor is not 64 bytes")
