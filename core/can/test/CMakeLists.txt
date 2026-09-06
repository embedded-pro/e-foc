add_executable(e_foc.can_test)
emil_build_for(e_foc.can_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.can_test)

target_link_libraries(e_foc.can_test PUBLIC
    gmock_main
    e_foc.can
    e_foc.state_machine
    e_foc.foc.cascade
    e_foc.platform_abstraction_mock
    e_foc.state_machine_mock
    e_foc.services.alignment_mock
    e_foc.services.electrical_system_ident_mock
    e_foc.services.mechanical_system_ident_mock
    e_foc.services.non_volatile_memory_mock
    e_foc.foc_mock
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
    can_lite.test_util
)

target_sources(e_foc.can_test PRIVATE
    TestCanLivenessWatchdog.cpp
    TestFocMotorCategoryServer.cpp
    TestFocMotorCategoryClient.cpp
    TestFocMotorCanBridge.cpp
    TestFocMotorCanClient.cpp
)

if (EMIL_ENABLE_COVERAGE)
    emil_enable_coverage_for(e_foc.can_test)
endif()
