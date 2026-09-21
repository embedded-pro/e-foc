add_library(e_foc.tool.simulator.view.gui ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.simulator.view.gui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.simulator.view.gui PUBLIC
    Qt6::Widgets
    ui.core
    ui.theme
    ui.scope
    ui.widgets
    ui.backend.qt
    e_foc.foc.model
    e_foc.foc.interfaces
    e_foc.state_machine
    infra.event
)

target_sources(e_foc.tool.simulator.view.gui PRIVATE
    QtOwned.hpp
    ControlPanel.hpp
    ControlPanel.cpp
    Gui.hpp
    Gui.cpp
    GuiSimulation.hpp
    GuiSimulation.cpp
    ParametersPanel.hpp
    ParametersPanel.cpp
    ScopesPanel.hpp
    ScopesPanel.cpp
    ScopeWidget.hpp
    ScopeWidget.cpp
    ScopeToolbar.hpp
    ScopeToolbar.cpp
    HexagonWidget.hpp
    HexagonWidget.cpp
)

set_target_properties(e_foc.tool.simulator.view.gui PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.view.gui PRIVATE /W4 /WX-)
else()
    target_compile_options(e_foc.tool.simulator.view.gui PRIVATE
        -Wno-error
    )
endif()
