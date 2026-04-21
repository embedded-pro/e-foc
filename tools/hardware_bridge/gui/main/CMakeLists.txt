add_executable(e_foc.tool.hardware_bridge)
emil_build_for(e_foc.tool.hardware_bridge HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_include_directories(e_foc.tool.hardware_bridge PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.tool.hardware_bridge PRIVATE
    e_foc.tool.hardware_bridge.gui
)

target_sources(e_foc.tool.hardware_bridge PRIVATE
    Main.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge PRIVATE
        -Wno-error
    )
endif()
