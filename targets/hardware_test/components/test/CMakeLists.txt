add_executable(e_foc.hardware_test.components.test)
emil_build_for(e_foc.hardware_test.components.test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.hardware_test.components.test)

target_link_libraries(e_foc.hardware_test.components.test PUBLIC
    gmock_main
    e_foc.hardware_test.components
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
    infra.util_test_helper
)

target_sources(e_foc.hardware_test.components.test PRIVATE
    TestTerminal.cpp
)
