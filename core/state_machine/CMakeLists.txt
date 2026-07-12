option(E_FOC_AUTO_TRANSITION_POLICY "Use automatic (programmatic) transition policy instead of CLI" OFF)

add_library(e_foc.state_machine STATIC)

target_include_directories(e_foc.state_machine PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.state_machine PUBLIC
    can_lite.categories.foc_motor
    e_foc.foc.implementations
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
    BoardProtectionFaultNotifier.hpp
    BoardProtectionFaultNotifier.cpp
    ControlMode.hpp
    ControlModeStateMachine.hpp
    ControlModeStateMachine.cpp
    FaultNotifier.hpp
    FocMotorCanBridge.hpp
    FocMotorCanBridge.cpp
    FocStateMachine.hpp
    FocStateMachineCommon.hpp
    FocStateMachineCommon.cpp
    OuterLoopStateMachine.hpp
    OuterLoopStateMachine.cpp
    PositionStateMachine.hpp
    PositionStateMachine.cpp
    SpeedStateMachine.hpp
    SpeedStateMachine.cpp
    TorqueStateMachine.hpp
    TorqueStateMachine.cpp
    TransitionPolicies.hpp
)

if (E_FOC_AUTO_TRANSITION_POLICY)
    target_compile_definitions(e_foc.state_machine PUBLIC
        E_FOC_AUTO_TRANSITION_POLICY
    )
endif()

add_subdirectory(test)
add_subdirectory(test_doubles)
