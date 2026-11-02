add_executable(e_foc.bootloader.main)

target_include_directories(e_foc.bootloader.main PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/${TARGET_MCU_VENDOR}"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.bootloader.main PRIVATE
    bootloader
    e_foc.platform_abstraction_${E_FOC_TARGET_BOARD}
)

target_sources(e_foc.bootloader.main PRIVATE
    KeySlot.cpp
    KeySlot.hpp
    Main.cpp
    ${TARGET_MCU_VENDOR}/BootloaderPlatform.cpp
    ${TARGET_MCU_VENDOR}/BootloaderPlatform.hpp
    ../ImageTarget.hpp
)

if(TARGET_MCU_VENDOR STREQUAL "ti")
    get_target_property(startupSource ti.hal_driver_${TARGET_MCU_FAMILY} HAL_TI_STARTUP_SOURCE)
    add_library(e_foc.bootloader.startup OBJECT ${startupSource})
    target_link_libraries(e_foc.bootloader.startup PRIVATE ti.hal_driver)
    set_target_properties(e_foc.bootloader.startup PROPERTIES COMPILE_WARNING_AS_ERROR Off)

    target_link_libraries(e_foc.bootloader.main PRIVATE
        $<TARGET_OBJECTS:e_foc.bootloader.startup>
        $<TARGET_OBJECTS:hal.cortex_m.runtime>
        hal.cortex_m
        ti.hal_driver
    )
    e_foc_ti_target_linker_scripts(e_foc.bootloader.main BOOTLOADER)
endif()

set(E_FOC_BOOTLOADER_KEYS "" CACHE FILEPATH "Key set written into the bootloader's key slot after the link; empty leaves the slot erased")
if(E_FOC_BOOTLOADER_KEYS)
    emil_bootloader_key_slot(e_foc.bootloader.main KEYS "${E_FOC_BOOTLOADER_KEYS}")
endif()

emil_generate_artifacts(TARGET e_foc.bootloader.main MAP BIN HEX)

