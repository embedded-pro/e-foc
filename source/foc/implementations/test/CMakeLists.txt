add_executable(e_foc.foc_test)
emil_build_for(e_foc.foc_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc_test)

target_link_libraries(e_foc.foc_test PUBLIC
    gmock_main
    e_foc.foc.implementations
    numerical.math_test_helper
    numerical.controllers.implementations
    infra.timer_test_helper
    e_foc.foc_mock
)

target_sources(e_foc.foc_test PRIVATE
    TestFocTorqueImpl.cpp
    TestFocWithSpeedPidImpl.cpp
    TestRealTimeFrictionAndInertiaEstimator.cpp
    TestSpaceVectorModulation.cpp
    TestSpeedController.cpp
    TestTorqueController.cpp
    TestTransformsClarkePark.cpp
    TestWithAutomaticCurrentPidGains.cpp
    TestWithRealTimeSpeedPidTune.cpp
)
