ENTRY(Reset_Handler)

REGION_ALIAS("REGION_TEXT", FLASH);
REGION_ALIAS("REGION_RAM", RAM);

_estack = ORIGIN(REGION_RAM) + LENGTH(REGION_RAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        _text = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > REGION_TEXT

    INCLUDE image_sections.ld

    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > REGION_TEXT

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > REGION_TEXT

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > REGION_TEXT

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > REGION_TEXT

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > REGION_RAM AT > REGION_TEXT

    _sidata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > REGION_RAM

    end = _ebss;

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
    }
}
