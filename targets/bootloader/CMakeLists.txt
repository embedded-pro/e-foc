get_property(bootloaderLdScripts GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS)
if(bootloaderLdScripts)
    add_subdirectory(main)
endif()
