add_executable(e_foc.bootloader.main)

target_include_directories(e_foc.bootloader.main PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/${TARGET_MCU_VENDOR}"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.bootloader.main PRIVATE
    e_foc.bootloader
    e_foc.platform_abstraction_${E_FOC_TARGET_BOARD}
)

target_sources(e_foc.bootloader.main PRIVATE
    JumpToApplication.hpp
    Main.cpp
    ${TARGET_MCU_VENDOR}/BootloaderPlatform.cpp
    ${TARGET_MCU_VENDOR}/BootloaderPlatform.hpp
    ../keys/DevelopmentKeys.hpp
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

emil_generate_artifacts(TARGET e_foc.bootloader.main MAP BIN HEX)

if(TARGET e_foc.sync_foc_sensored.main)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(bootloaderHex "$<TARGET_FILE_DIR:e_foc.bootloader.main>/$<TARGET_FILE_BASE_NAME:e_foc.bootloader.main>.hex")
    set(applicationHex "$<TARGET_FILE_DIR:e_foc.sync_foc_sensored.main>/$<TARGET_FILE_BASE_NAME:e_foc.sync_foc_sensored.main>.hex")
    set(combinedHex "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/e_foc.sync_foc_sensored.main.combined.hex")

    add_custom_command(OUTPUT "${combinedHex}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tools/firmware_image.py" combine --output "${combinedHex}" "${bootloaderHex}" "${applicationHex}"
        DEPENDS e_foc.bootloader.main e_foc.sync_foc_sensored.main
        VERBATIM
    )
    add_custom_target(e_foc.sync_foc_sensored.combined ALL DEPENDS "${combinedHex}")
endif()
