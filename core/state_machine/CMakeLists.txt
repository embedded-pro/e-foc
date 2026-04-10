add_library(e_foc.state_machine INTERFACE)

target_include_directories(e_foc.state_machine INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.state_machine INTERFACE
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

target_sources(e_foc.state_machine INTERFACE
    FaultNotifier.hpp
    FocStateMachine.hpp
    FocStateMachineImpl.hpp
    TransitionPolicies.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
