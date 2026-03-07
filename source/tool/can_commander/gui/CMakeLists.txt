add_library(e_foc.tool.can_commander.gui ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.can_commander.gui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.can_commander.gui PUBLIC
    Qt6::Widgets
    e_foc.services.can_protocol.core
    e_foc.tool.can_commander.adapter
    e_foc.tool.can_commander.logic
)

target_sources(e_foc.tool.can_commander.gui PRIVATE
    MainWindow.hpp
    MainWindow.cpp
    ConnectionPanel.hpp
    ConnectionPanel.cpp
    CommandPanel.hpp
    CommandPanel.cpp
    TelemetryPanel.hpp
    TelemetryPanel.cpp
)

set_target_properties(e_foc.tool.can_commander.gui PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander.gui PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander.gui PRIVATE
        -Wno-error
    )
endif()
