add_executable(e_foc.services.standstill_test)
emil_build_for(e_foc.services.standstill_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.standstill_test)

target_link_libraries(e_foc.services.standstill_test PUBLIC
    gmock_main
    e_foc.services.standstill
    e_foc.drivers_mock
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(e_foc.services.standstill_test PRIVATE
    TestRotorStandstill.cpp
)
