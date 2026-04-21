add_executable(e_foc.tool.hardware_bridge.bridge_client_headless)
emil_build_for(e_foc.tool.hardware_bridge.bridge_client_headless HOST All)

target_include_directories(e_foc.tool.hardware_bridge.bridge_client_headless PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.tool.hardware_bridge.bridge_client_headless PRIVATE
    e_foc.tool.hardware_bridge.can_client
    e_foc.tool.hardware_bridge.serial_client
    e_foc.tool.hardware_bridge.common
    services.network_instantiations
    args
)

target_sources(e_foc.tool.hardware_bridge.bridge_client_headless PRIVATE
    Main.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.bridge_client_headless PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.bridge_client_headless PRIVATE
        -Wno-error
    )
endif()
