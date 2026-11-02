add_subdirectory(${E_FOC_TARGET_BOARD})
add_subdirectory(implementation)

# Uses the infra mem file but the e-foc custom sections.ld (adds .error_handling). On a board with a bootloader,
# the bootloader links at the start of flash
function(e_foc_ti_target_linker_scripts target)
    cmake_parse_arguments(PARSE_ARGV 1 E_FOC_LAYOUT "BOOTLOADER" "" "")
    get_property(bootloaderLdScripts GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS)

    set(sectionsLd "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ldscripts/sections.ld")
    if(bootloaderLdScripts AND E_FOC_LAYOUT_BOOTLOADER)
        set(scripts "${bootloaderLdScripts}/mem_bootloader.ld" "${bootloaderLdScripts}/flash_layout.ld" "${sectionsLd}")
    else()
        set(scripts "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../infra/hal/ti/tiva/ldscripts/mem_${TARGET_MCU}.ld" "${sectionsLd}")
    endif()

    foreach(script IN LISTS scripts)
        target_link_options(${target} PUBLIC "LINKER:-T${script}")
    endforeach()
    set_property(TARGET ${target} APPEND PROPERTY INTERFACE_LINK_DEPENDS ${scripts})
endfunction()
