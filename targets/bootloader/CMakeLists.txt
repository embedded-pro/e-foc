get_property(bootloaderLdScripts GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS)

if(DEFINED E_FOC_BOARD)
    set(declared ${E_FOC_${E_FOC_BOARD}_BOOTLOADER})
    if((declared AND NOT bootloaderLdScripts) OR (bootloaderLdScripts AND NOT declared))
        message(FATAL_ERROR "cmake/ defines ${E_FOC_BOARD} with bootloader ${declared}, but its platform's bootloader layout says otherwise")
    endif()
endif()

if(bootloaderLdScripts)
    add_subdirectory(main)
endif()
