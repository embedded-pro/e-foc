add_library(e_foc.tool.virtual_motor.app.gui ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.virtual_motor.app.gui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
)

target_link_libraries(e_foc.tool.virtual_motor.app.gui PUBLIC
    Qt6::Widgets
    ui.theme
    ui.backend.qt
    e_foc.tool.virtual_motor.app.logic
    e_foc.tool.virtual_motor.platform
)

target_sources(e_foc.tool.virtual_motor.app.gui PRIVATE
    AddMotorDialog.cpp
    AddMotorDialog.hpp
    CaptureBar.cpp
    CaptureBar.hpp
    LogView.cpp
    LogView.hpp
    ManagerWindow.cpp
    ManagerWindow.hpp
    MotorWindow.cpp
    MotorWindow.hpp
    QtEngineProcesses.cpp
    QtEngineProcesses.hpp
    QtRigStore.cpp
    QtRigStore.hpp
    ScopeArea.cpp
    ScopeArea.hpp
    TorqueNoisePanel.cpp
    TorqueNoisePanel.hpp
)

set_target_properties(e_foc.tool.virtual_motor.app.gui PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.virtual_motor.app.gui PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.virtual_motor.app.gui PRIVATE -Wno-error)
endif()

add_subdirectory(test)
