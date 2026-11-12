add_library(e_foc.tool.can_commander.logic ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.can_commander.logic PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.can_commander.logic PUBLIC
    e_foc.boards
    e_foc.can
    upgrade.staging
    can_lite.categories.firmware_upgrade
    can_lite.drivers.interface
)

target_sources(e_foc.tool.can_commander.logic PRIVATE
    CanCommandClient.hpp
    CanCommandClient.cpp
    CommandDescriptions.hpp
    CommandDescriptions.cpp
    CommandTracker.hpp
    CommandTracker.cpp
    FaultRecordAssembler.hpp
    FaultRecordAssembler.cpp
    FaultHistoryText.hpp
    FaultHistoryText.cpp
    FirmwareIdentityCollector.hpp
    FirmwareIdentityCollector.cpp
    FirmwarePack.hpp
    FirmwarePack.cpp
    FirmwareRefusal.hpp
    FirmwareRefusal.cpp
    FirmwareTransfer.hpp
    FirmwareTransfer.cpp
    MotorCommands.hpp
    MotorCommands.cpp
    MotorSettings.hpp
    MotorSettings.cpp
    NodeData.hpp
    NodeData.cpp
    ReadScheduler.hpp
    ReadScheduler.cpp
    RecoveryAdvice.hpp
    RecoveryAdvice.cpp
    SetpointAxis.hpp
    SetpointAxis.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander.logic PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander.logic PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
