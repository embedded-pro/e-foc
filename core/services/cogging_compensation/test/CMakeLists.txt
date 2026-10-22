add_executable(e_foc.services.cogging_compensation_test)
emil_build_for(e_foc.services.cogging_compensation_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.cogging_compensation_test)

target_link_libraries(e_foc.services.cogging_compensation_test PUBLIC
    gmock_main
    e_foc.services.cogging_compensation
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
    e_foc.foc_mock
)

target_sources(e_foc.services.cogging_compensation_test PRIVATE
    TestCoggingMapBuilder.cpp
    TestCoggingMappingImpl.cpp
)
