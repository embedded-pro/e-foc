add_library(e_foc.platform_abstraction_cortex_m_common INTERFACE)

target_include_directories(e_foc.platform_abstraction_cortex_m_common INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction_cortex_m_common INTERFACE
    e_foc.foc.interfaces
    hal.cortex_m
)

function(e_foc_add_application_image target)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplicationImage.cpp")
    target_compile_definitions(${target} PRIVATE
        E_FOC_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
        E_FOC_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
        E_FOC_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
    )

    emil_bootloader_image_descriptor(${target})
endfunction()
