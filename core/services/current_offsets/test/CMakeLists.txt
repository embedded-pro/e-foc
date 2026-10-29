add_executable(e_foc.services.current_offsets_test)
emil_build_for(e_foc.services.current_offsets_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.current_offsets_test)

target_link_libraries(e_foc.services.current_offsets_test PUBLIC
    gmock_main
    e_foc.services.current_offsets
    e_foc.services.current_offsets_mock
    e_foc.drivers_mock
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(e_foc.services.current_offsets_test PRIVATE
    TestConfirmedPhaseCurrentOffsets.cpp
    TestPhaseCurrentOffsetMeasurement.cpp
)
