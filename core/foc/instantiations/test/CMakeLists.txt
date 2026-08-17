add_executable(e_foc.foc.instantiations_test)
emil_build_for(e_foc.foc.instantiations_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.instantiations_test)

target_link_libraries(e_foc.foc.instantiations_test PUBLIC
    gmock_main
    e_foc.foc.instantiations
    e_foc.foc_mock
    e_foc.drivers_mock
    infra.event_test_helper
)

target_sources(e_foc.foc.instantiations_test PRIVATE
    TestLowPriorityInterruptImpl.cpp
    TestRunner.cpp
)
