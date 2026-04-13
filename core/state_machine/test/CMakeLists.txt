add_executable(e_foc.state_machine_test)
emil_build_for(e_foc.state_machine_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.state_machine_test)

target_link_libraries(e_foc.state_machine_test PUBLIC
    gmock_main
    e_foc.state_machine
    e_foc.foc.implementations
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
    e_foc.foc_mock
    e_foc.state_machine_mock
    e_foc.services.alignment_mock
    e_foc.services.electrical_system_ident_mock
    e_foc.services.mechanical_system_ident_mock
    e_foc.services.non_volatile_memory_mock
)

target_sources(e_foc.state_machine_test PRIVATE
    TestFaultNotifier.cpp
    TestFocStateMachineTorque.cpp
    TestFocStateMachineSpeed.cpp
    TestFocStateMachinePosition.cpp
)

if (EMIL_ENABLE_COVERAGE)
    emil_enable_coverage_for(e_foc.state_machine_test)
endif()
