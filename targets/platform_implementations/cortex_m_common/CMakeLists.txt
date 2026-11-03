add_library(e_foc.platform_abstraction_cortex_m_common INTERFACE)

target_include_directories(e_foc.platform_abstraction_cortex_m_common INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction_cortex_m_common INTERFACE
    e_foc.foc.interfaces
    hal.cortex_m
)

add_version_header_target(e_foc.version)

function(e_foc_add_application_image target)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplicationImage.cpp")
    add_version_header_dependency(${target} e_foc.version)

    emil_bootloader_image_descriptor(${target})
endfunction()
