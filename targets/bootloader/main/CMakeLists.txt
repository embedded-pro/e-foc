add_executable(e_foc.bootloader.main)

if(E_FOC_TARGET_PLATFORM STREQUAL "qemu")
    set(platformDir qemu)
    set(boardLibrary e_foc.platform_abstraction_qemu_board)
else()
    set(platformDir ${TARGET_MCU_VENDOR})
    set(boardLibrary e_foc.platform_abstraction_${E_FOC_TARGET_BOARD})
endif()

target_include_directories(e_foc.bootloader.main PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/${platformDir}"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.bootloader.main PRIVATE
    bootloader
    e_foc.platform_abstraction_cortex_m_common
    ${boardLibrary}
)

target_sources(e_foc.bootloader.main PRIVATE
    KeySlot.cpp
    KeySlot.hpp
    Main.cpp
    ${platformDir}/BootloaderPlatform.cpp
    ${platformDir}/BootloaderPlatform.hpp
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
    target_compile_definitions(e_foc.bootloader.main PRIVATE
        HARDWARE_CLOCKS_HEADER="targets/platform_implementations/ti/${E_FOC_TARGET_BOARD}/Clocks.hpp"
    )
    e_foc_ti_target_linker_scripts(e_foc.bootloader.main BOOTLOADER)
elseif(E_FOC_TARGET_PLATFORM STREQUAL "qemu")
    target_link_libraries(e_foc.bootloader.main PRIVATE e_foc.qemu.semihosting)
    e_foc_link_qemu_runtime(e_foc.bootloader.main)
    e_foc_qemu_target_linker_scripts(e_foc.bootloader.main BOOTLOADER)
endif()

emil_fetch_bootloader_image()

emil_generate_artifacts(TARGET e_foc.bootloader.main MAP BIN HEX)
