add_library(e_foc.tool.hardware_bridge.gui ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.hardware_bridge.gui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.hardware_bridge.gui PUBLIC
    Qt6::Network
    Qt6::Widgets
    e_foc.tool.hardware_bridge.serial_client
    e_foc.tool.hardware_bridge.can_client
    e_foc.tool.hardware_bridge.terminal
    services.network_instantiations
)

target_sources(e_foc.tool.hardware_bridge.gui PRIVATE
    AnsiTerminalWidget.hpp
    AnsiTerminalWidget.cpp
    BridgeController.hpp
    BridgeController.cpp
    BridgeWindow.hpp
    BridgeWindow.cpp
    SerialPanel.hpp
    SerialPanel.cpp
    CanPanel.hpp
    CanPanel.cpp
)

set_target_properties(e_foc.tool.hardware_bridge.gui PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.gui PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.gui PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(main)
