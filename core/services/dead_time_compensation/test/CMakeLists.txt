add_executable(e_foc.services.dead_time_compensation_test)
emil_build_for(e_foc.services.dead_time_compensation_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.dead_time_compensation_test)

target_link_libraries(e_foc.services.dead_time_compensation_test PUBLIC
    gmock_main
    e_foc.services.dead_time_compensation
    e_foc.drivers_mock
    e_foc.foc.model
)

target_sources(e_foc.services.dead_time_compensation_test PRIVATE
    TestDeadTimeCompensation.cpp
)
