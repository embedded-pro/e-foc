__stack = ORIGIN(RAM) + LENGTH(RAM);
_estack = __stack;

__Main_Stack_Size = 1024;
PROVIDE(_Main_Stack_Size = __Main_Stack_Size);

__Main_Stack_Limit = __stack - __Main_Stack_Size;
PROVIDE(_Main_Stack_Limit = __Main_Stack_Limit);

PROVIDE(_Heap_Begin = __end__);
PROVIDE(_Heap_Limit = __stack - __Main_Stack_Size);

EXTERN(__isr_vectors)

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector : ALIGN(4)
    {
        FILL(0xFF)

        KEEP(*(.isr_vector))
        *(.after_vectors .after_vectors.*)
    } >FLASH

    .inits : ALIGN(4)
    {
        KEEP(*(.init))
        KEEP(*(.fini))
        _fini = .;

        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array_sysinit .preinit_array_sysinit.*))
        KEEP(*(.preinit_array_platform .preinit_array_platform.*))
        KEEP(*(.preinit_array .preinit_array.*))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } >FLASH

    .text : ALIGN(4)
    {
        _text = .;
        *(.text .text.*)

        KEEP(*(.eh_frame*))

        *(.glue_7)
        *(.glue_7t)

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata : ALIGN(4)
    {
        *(.rodata)
        *(.rodata*)
    } >FLASH

    /* Exception sections */
    .ARM.extab : ALIGN(4)
    {
       *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        . = ALIGN(4);
        __exidx_end = .;
    } > FLASH

    _sidata = LOADADDR(.data);

    .data : ALIGN(4)
    {
        FILL(0xFF)
        _sdata = .;
        __data_start__ = _sdata;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
        __data_end__ = _edata;
    } >RAM AT>FLASH

    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* Persistent 1 KiB region for fault data — not zeroed on reset */
    .error_handling (NOLOAD) : ALIGN(4)
    {
        _error_handling_start = .;
        KEEP(*(.error_handling .error_handling.*))
        . = ALIGN(4);
        _error_handling_end = .;
    } >RAM
    ASSERT((_error_handling_end - _error_handling_start) <= 1024,
           "error_handling section exceeds 1 KiB")

    .noinit (NOLOAD) : ALIGN(4)
    {
        _noinit = .;
        *(.noinit .noinit.*)
        . = ALIGN(4);
        _end_noinit = .;
    } >RAM

    PROVIDE(__end__ = _end_noinit);

    MAPPING_TABLE (NOLOAD) : { *(MAPPING_TABLE) } >RAM_SHARED
    MB_MEM1 (NOLOAD)       : { *(MB_MEM1) } >RAM_SHARED
    MB_MEM2 (NOLOAD)       : { _sMB_MEM2 = . ; *(MB_MEM2) ; _eMB_MEM2 = . ; } >RAM_SHARED

    ._check_stack : ALIGN(4)
    {
        . = . + __Main_Stack_Size;
    } >RAM

    /* Stabs debugging sections */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }

    /* DWARF debug sections */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
}
