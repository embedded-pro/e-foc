add_library(e_foc.tool.can_commander.gui ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.can_commander.gui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.can_commander.gui PUBLIC
    Qt6::Widgets
    ui.core
    ui.theme
    ui.model
    ui.shell
    ui.backend.qt
    can_lite.drivers.interface
    e_foc.tool.can_commander.logic
    e_foc.tool.can_commander.support
)

target_sources(e_foc.tool.can_commander.gui PRIVATE
    CommandPanel.hpp
    CommandPanel.cpp
    ConfigurationTab.hpp
    ConfigurationTab.cpp
    ConnectionPanel.hpp
    ConnectionPanel.cpp
    ControllersTab.hpp
    ControllersTab.cpp
    DiagnosticsTab.hpp
    DiagnosticsTab.cpp
    DisplayFormat.hpp
    EventLogView.hpp
    EventLogView.cpp
    MainWindow.hpp
    MainWindow.cpp
    MotorOverview.hpp
    MotorOverview.cpp
    MotorPanel.hpp
    MotorPanel.cpp
    MotorParametersTab.hpp
    MotorParametersTab.cpp
    SettingEditor.hpp
    SettingEditor.cpp
    SettingsPage.hpp
    SettingsPage.cpp
    TelemetryPanel.hpp
    TelemetryPanel.cpp
    UpdateBanner.hpp
    UpdateBanner.cpp
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
