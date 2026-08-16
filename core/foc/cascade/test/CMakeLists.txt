add_executable(e_foc.foc.cascade_test)
emil_build_for(e_foc.foc.cascade_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.cascade_test)

target_link_libraries(e_foc.foc.cascade_test PUBLIC
    gmock_main
    e_foc.foc.cascade
    e_foc.foc_mock
    numerical.math_test_helper
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(e_foc.foc.cascade_test PRIVATE
    TestPositionCascade.cpp
    TestSpeedCascade.cpp
    TestTorqueCascade.cpp
)
