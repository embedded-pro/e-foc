add_library(e_foc.tool.virtual_motor.platform ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.tool.virtual_motor.platform PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
)

target_link_libraries(e_foc.tool.virtual_motor.platform PUBLIC
    e_foc.boards
    e_foc.foc.model.drive
    e_foc.foc.model.drive.memory
    e_foc.platform_abstraction
    e_foc.tool.virtual_motor.port
    can_lite.drivers.slcan
    can_lite.drivers.virtual
    infra.event
    infra.timer
    services.tracer
    services.util
)

target_sources(e_foc.tool.virtual_motor.platform PRIVATE
    CalibratedMemory.cpp
    CalibratedMemory.hpp
    BoardPeripherals.cpp
    BoardPeripherals.hpp
    CaptureRecorder.cpp
    CaptureRecorder.hpp
    FileEeprom.cpp
    FileEeprom.hpp
    InstrumentReports.hpp
    Pacer.cpp
    Pacer.hpp
    PlatformPortTarget.cpp
    PlatformPortTarget.hpp
    PortSocket.cpp
    PortSocket.hpp
    ProfileRunner.cpp
    ProfileRunner.hpp
    PlantDescription.cpp
    PlantDescription.hpp
    PlantDisturbances.cpp
    PlantDisturbances.hpp
    PlantInstruments.cpp
    PlantInstruments.hpp
    PlantPortClient.cpp
    PlantPortClient.hpp
    PlantPowerStage.cpp
    PlantPowerStage.hpp
    PlantSignals.cpp
    PlantSignals.hpp
    RunLoop.cpp
    RunLoop.hpp
    StreamWindow.cpp
    StreamWindow.hpp
    TriggerWatch.cpp
    TriggerWatch.hpp
    VirtualCanPort.cpp
    VirtualCanPort.hpp
    VirtualPlatform.cpp
    VirtualPlatform.hpp
)

add_subdirectory(test)
