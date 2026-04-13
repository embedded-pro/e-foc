PROVIDE(_Heap_Begin = end);
PROVIDE(_Heap_Limit = _estack - __Main_Stack_Size);
PROVIDE(_estack = ORIGIN(REGION_RAM) + LENGTH(REGION_RAM));

__Main_Stack_Size = 1024;

ENTRY(Reset_Handler)

REGION_ALIAS("REGION_TEXT", FLASH);
REGION_ALIAS("REGION_RAM", RAM);

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        _text = .;
        KEEP(*(.isr_vector))
        *(.text .text* .gnu.linkonce.t.*)
        *(.glue_7t) *(.glue_7)
        *(.rodata .rodata* .gnu.linkonce.r.*)
        *(.ARM.extab* .gnu.linkonce.armextab.*)

        . = ALIGN(4);
        KEEP(*(.init))
        . = ALIGN(4);
        __preinit_array_start = .;
        KEEP (*(SORT(.preinit_array*)))
        KEEP (*(.preinit_array))
        __preinit_array_end = .;

        . = ALIGN(4);
        __init_array_start = .;
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        __init_array_end = .;

        . = ALIGN(4);
        KEEP(*(.fini))

        . = ALIGN(4);
        __fini_array_start = .;
        KEEP (*(.fini_array))
        KEEP (*(SORT(.fini_array.*)))
        __fini_array_end = .;

        . = ALIGN(4);
        _etext = .;
    } > REGION_TEXT

    .data : AT(ADDR(.text) + SIZEOF(.text))
    {
        . = ALIGN(4);
        _data = .;
        *(vtable)
        *(.data .data* .gnu.linkonce.d.*)
        _edata = .;
    } > REGION_RAM

    .bss (NOLOAD):
    {
        . = ALIGN(4);
        _bss = .;
        *(.bss .bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        . = ALIGN(8);
    } > REGION_RAM

    /* Persistent 1 KiB region for fault data — not zeroed on reset */
    .error_handling (NOLOAD):
    {
        . = ALIGN(4);
        _error_handling_start = .;
        KEEP(*(.error_handling .error_handling.*))
        . = ALIGN(4);
        _error_handling_end = .;
    } > REGION_RAM
    ASSERT((_error_handling_end - _error_handling_start) <= 1024,
           "error_handling section exceeds 1 KiB")

    PROVIDE_HIDDEN (__exidx_start = .);
    .ARM.exidx :
    {
      *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > REGION_RAM
    PROVIDE_HIDDEN (__exidx_end = .);

    . = ALIGN(4);
    end = . ;
    _end = . ;
}
