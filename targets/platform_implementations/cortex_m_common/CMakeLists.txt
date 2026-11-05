add_library(e_foc.platform_abstraction_cortex_m_common INTERFACE)

target_include_directories(e_foc.platform_abstraction_cortex_m_common INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction_cortex_m_common INTERFACE
    e_foc.foc.interfaces
    hal.cortex_m
)

function(e_foc_flash_layout_address output layout symbol)
    file(STRINGS "${layout}" line REGEX "^${symbol} = ")
    if(NOT line MATCHES "^${symbol} = (0x[0-9A-Fa-f]+|[0-9]+)(K?)")
        message(FATAL_ERROR "${layout} does not set ${symbol}")
    endif()

    if(CMAKE_MATCH_2 STREQUAL "K")
        math(EXPR address "${CMAKE_MATCH_1} * 1024")
    else()
        math(EXPR address "${CMAKE_MATCH_1}")
    endif()
    set(${output} ${address} PARENT_SCOPE)
endfunction()

function(e_foc_add_upgrade_pack_script target)
    set(product "${E_FOC_${E_FOC_BOARD}_IDENTIFICATION}")
    set(imageTarget ${E_FOC_${E_FOC_BOARD}_BOARD_KIND} ${E_FOC_${E_FOC_BOARD}_DEVICE_FAMILY} ${E_FOC_${E_FOC_BOARD}_DEVICE_IDENTITY})
    get_property(ldscripts GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS)

    set(layout "${ldscripts}/flash_layout.ld")
    e_foc_flash_layout_address(slotStart "${layout}" __application_slot_start)
    e_foc_flash_layout_address(slotEnd "${layout}" __application_slot_end)
    e_foc_flash_layout_address(stagingStart "${layout}" __staging_slot_start)
    e_foc_flash_layout_address(stagingEnd "${layout}" __staging_slot_end)
    math(EXPR stagingSize "${stagingEnd} - ${stagingStart}")
    string(REGEX REPLACE "^e_foc\\.(.+)\\.main$" "\\1" component "${target}")

    file(GENERATE OUTPUT "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.upgrade_pack.cmake" CONTENT
"set(TOOL \"$<TARGET_FILE:application.bootloader_image>\")
set(IMAGE \"$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.image.bin\")
set(PRODUCT \"${product}\")
set(PRODUCT_VERSION ${CMAKE_PROJECT_VERSION})
set(COMPONENT ${component})
set(IMAGE_TARGET ${imageTarget})
set(SLOT_START ${slotStart})
set(SLOT_END ${slotEnd})
set(STAGING_SIZE ${stagingSize})
include(\"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/UpgradePack.cmake\")
")
endfunction()

function(e_foc_add_application_image target)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplicationImage.cpp")
    add_version_header_dependency(${target} e_foc.version_header)

    emil_bootloader_image_descriptor(${target})
    e_foc_add_upgrade_pack_script(${target})
endfunction()
