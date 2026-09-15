add_library(e_foc.state_machine STATIC)

target_include_directories(e_foc.state_machine PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.state_machine PUBLIC
    can_lite.core
    e_foc.foc.cascade
    e_foc.foc.instantiations
    e_foc.platform_abstraction
    e_foc.services.alignment
    e_foc.services.cli
    e_foc.services.electrical_system_ident
    e_foc.services.mechanical_system_ident
    e_foc.services.non_volatile_memory
    infra.util
    services.tracer
    services.util
)

target_sources(e_foc.state_machine PRIVATE
    AlgorithmPersistence.hpp
    AlgorithmPersistence.cpp
    CalibrationContext.hpp
    CalibrationContext.cpp
    CalibrationOrchestrator.hpp
    CalibrationOrchestrator.cpp
    ControlMode.hpp
    ControlModeCliCommands.hpp
    ControlModeCliCommands.cpp
    ControlModeStateMachine.hpp
    ControlModeStateMachine.cpp
    FaultController.hpp
    FaultController.cpp
    FaultNotifier.hpp
    FocStateMachine.hpp
    FocStateMachineImpl.cpp
    FocStateMachineCommon.hpp
    FocStateMachineCommon.cpp
    FocStateMachineCommonCalibration.cpp
    OuterLoopStateMachine.hpp
    OuterLoopStateMachine.cpp
    OuterLoopStateMachineFor.hpp
    PlatformFaultNotifier.hpp
    PlatformFaultNotifier.cpp
    PositionStateMachine.hpp
    SpeedStateMachine.hpp
    TorqueStateMachine.hpp
    TorqueStateMachine.cpp
    TransitionPolicies.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
