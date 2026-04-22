add_executable(e_foc.tool.hardware_bridge.bridge_client_gui)
emil_build_for(e_foc.tool.hardware_bridge.bridge_client_gui HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_include_directories(e_foc.tool.hardware_bridge.bridge_client_gui PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../..>"
)

target_link_libraries(e_foc.tool.hardware_bridge.bridge_client_gui PRIVATE
    e_foc.tool.hardware_bridge.gui
)

target_sources(e_foc.tool.hardware_bridge.bridge_client_gui PRIVATE
    Main.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.bridge_client_gui PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.bridge_client_gui PRIVATE
        -Wno-error
    )
endif()
