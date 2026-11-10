add_library(e_foc.tool.virtual_motor.app.logic ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.tool.virtual_motor.app.logic PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
)

target_link_libraries(e_foc.tool.virtual_motor.app.logic PUBLIC
    e_foc.tool.virtual_motor.port
    can_lite.drivers.virtual
    numerical.math
    ui.core
    ui.theme
    ui.widgets
)

target_sources(e_foc.tool.virtual_motor.app.logic PRIVATE
    CaptureAssembly.cpp
    CaptureAssembly.hpp
    CaptureCsv.cpp
    CaptureCsv.hpp
    EncoderView.cpp
    EncoderView.hpp
    EngineProcesses.hpp
    ModulationView.cpp
    ModulationView.hpp
    MotorLink.cpp
    MotorLink.hpp
    MotorText.cpp
    MotorText.hpp
    MotorSpec.cpp
    MotorSpec.hpp
    PortListener.cpp
    PortListener.hpp
    Rig.cpp
    Rig.hpp
    RotorView.cpp
    RotorView.hpp
    SourceView.cpp
    SourceView.hpp
    TimeAxis.cpp
    TimeAxis.hpp
    TraceHistory.cpp
    TraceHistory.hpp
    TraceSignals.cpp
    TraceSignals.hpp
    TraceSources.cpp
    TraceSources.hpp
    TraceView.cpp
    TraceView.hpp
)

add_subdirectory(test)
