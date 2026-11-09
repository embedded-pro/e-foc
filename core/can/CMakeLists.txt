add_library(e_foc.can STATIC)

target_include_directories(e_foc.can PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.can PUBLIC
    can_lite.core
    can_lite.server
    can_lite.client
    can_lite.tracing_protocol
    can_lite.tracing
    e_foc.boards
    e_foc.foc.math
    e_foc.services.fault_history
    can_lite.categories.firmware_upgrade.session
    e_foc.state_machine
    services.tracer
)

target_sources(e_foc.can PRIVATE
    FocMotorMessages.hpp
    FocMotorWireContract.hpp
    FocMotorSettings.hpp
    FocMotorSettings.cpp
    FocMotorCategoryServer.hpp
    FocMotorCategoryServer.cpp
    FocMotorCategoryClient.hpp
    FocMotorCategoryClient.cpp
    FocMotorCanBridge.hpp
    FocMotorCanBridge.cpp
    FirmwareUpgradeInterlock.hpp
    FirmwareUpgradeInterlock.cpp
    FocMotorCommandOutcome.hpp
    FocMotorCommissioningBridge.hpp
    FocMotorCommissioningBridge.cpp
    FocMotorSettingsBridge.hpp
    FocMotorSettingsBridge.cpp
    FocMotorCanClient.hpp
    FocMotorCanClient.cpp
    FocMotorCurrentStreamer.hpp
    FocMotorCurrentStreamer.cpp
    CanCommandTimeout.hpp
    CanCommandTimeout.cpp
    CanLivenessWatchdog.hpp
    CanLivenessWatchdog.cpp
)

add_subdirectory(test)
