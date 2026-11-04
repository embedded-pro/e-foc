add_library(e_foc.platform_abstraction_qemu_board INTERFACE)

target_link_libraries(e_foc.platform_abstraction_qemu_board INTERFACE
    e_foc.boards
)

set_property(GLOBAL PROPERTY E_FOC_BOOTLOADER_LDSCRIPTS "${CMAKE_CURRENT_LIST_DIR}/ldscripts")

function(e_foc_qemu_target_linker_scripts target)
    cmake_parse_arguments(PARSE_ARGV 1 E_FOC_LAYOUT "APPLICATION;BOOTLOADER" "" "")

    set(ldscripts "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ldscripts")
    set(common "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../cortex_m_common/ldscripts")
    if(E_FOC_LAYOUT_BOOTLOADER)
        set(script "${ldscripts}/bootloader.ld")
        set(imageSectionsDir "${common}/standalone")
    else()
        set(script "${ldscripts}/application.ld")
        set(imageSectionsDir "${common}/application")
        e_foc_add_application_image(${target})
    endif()

    set_target_properties(${target} PROPERTIES EMIL_QEMU_LINKER_SCRIPT "${script}")
    target_link_options(${target} PRIVATE "LINKER:-L${imageSectionsDir}" "LINKER:-L${ldscripts}" "LINKER:-L${common}")
    file(GLOB scripts "${ldscripts}/*.ld")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${scripts} "${imageSectionsDir}/image_sections.ld" "${common}/bootloader_keys.ld")
endfunction()

add_subdirectory(implementation)
