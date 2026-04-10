option(E_FOC_AUTO_TRANSITION_POLICY "Use automatic (programmatic) transition policy instead of CLI" OFF)

if (EMIL_ENABLE_COVERAGE)
    add_library(e_foc.state_machine)
    set(E_FOC_STATE_MACHINE_VISIBILITY PUBLIC)
else()
    add_library(e_foc.state_machine INTERFACE)
    set(E_FOC_STATE_MACHINE_VISIBILITY INTERFACE)
endif()

target_include_directories(e_foc.state_machine ${E_FOC_STATE_MACHINE_VISIBILITY}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.state_machine ${E_FOC_STATE_MACHINE_VISIBILITY}
    infra.util
    e_foc.foc.instantiations
    e_foc.services.cli
    e_foc.services.non_volatile_memory
    e_foc.services.alignment
    e_foc.services.electrical_system_ident
    e_foc.services.mechanical_system_ident
    services.tracer
    services.util
)

target_sources(e_foc.state_machine ${E_FOC_STATE_MACHINE_VISIBILITY}
    FaultNotifier.hpp
    FocStateMachine.hpp
    FocStateMachineImpl.hpp
    TransitionPolicies.hpp
)

if (EMIL_ENABLE_COVERAGE)
    target_sources(e_foc.state_machine PRIVATE FocStateMachineImpl.cpp)
    target_compile_definitions(e_foc.state_machine PUBLIC E_FOC_STATE_MACHINE_COVERAGE_BUILD)
endif()

if (E_FOC_AUTO_TRANSITION_POLICY)
    target_compile_definitions(e_foc.state_machine ${E_FOC_STATE_MACHINE_VISIBILITY}
        E_FOC_AUTO_TRANSITION_POLICY
    )
endif()

add_subdirectory(test)
add_subdirectory(test_doubles)
