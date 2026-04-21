add_executable(e_foc.tool.hardware_bridge.serial_bridge)
emil_build_for(e_foc.tool.hardware_bridge.serial_bridge HOST All)

target_include_directories(e_foc.tool.hardware_bridge.serial_bridge PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.tool.hardware_bridge.serial_bridge PRIVATE
    e_foc.tool.hardware_bridge.serial_client
    services.network_instantiations
    args
)

target_sources(e_foc.tool.hardware_bridge.serial_bridge PRIVATE
    Main.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.serial_bridge PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.serial_bridge PRIVATE
        -Wno-error
    )
endif()
