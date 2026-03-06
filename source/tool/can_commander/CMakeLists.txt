add_executable(e_foc.tool.can_commander)
emil_build_for(e_foc.tool.can_commander HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_include_directories(e_foc.tool.can_commander PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.tool.can_commander PRIVATE
    Qt6::Widgets
    e_foc.services.can_protocol
)

target_sources(e_foc.tool.can_commander PRIVATE
    main/Main.cpp
    adapter/CanBusAdapter.hpp
    adapter/SocketCanAdapter.hpp
    adapter/SocketCanAdapter.cpp
    logic/CanCommandClient.hpp
    logic/CanCommandClient.cpp
    gui/MainWindow.hpp
    gui/MainWindow.cpp
    gui/ConnectionPanel.hpp
    gui/ConnectionPanel.cpp
    gui/CommandPanel.hpp
    gui/CommandPanel.cpp
    gui/TelemetryPanel.hpp
    gui/TelemetryPanel.cpp
)

set_target_properties(e_foc.tool.can_commander PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander PRIVATE
        -Wno-error
    )
endif()
