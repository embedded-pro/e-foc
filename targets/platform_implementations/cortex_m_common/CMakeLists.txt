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
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplicationImage.cpp")
    target_compile_definitions(${target} PRIVATE
        E_FOC_VERSION_MAJOR=${CMAKE_PROJECT_VERSION_MAJOR}
        E_FOC_VERSION_MINOR=${CMAKE_PROJECT_VERSION_MINOR}
        E_FOC_VERSION_PATCH=${CMAKE_PROJECT_VERSION_PATCH}
    )

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../bootloader/tools/firmware_image.py" stamp --objcopy "${EMIL_OBJ_COPY_TOOL}" "$<TARGET_FILE:${target}>"
        VERBATIM
    )
endfunction()
