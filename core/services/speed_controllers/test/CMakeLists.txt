add_executable(e_foc.services.speed_controllers_test)
emil_build_for(e_foc.services.speed_controllers_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.speed_controllers_test)

target_link_libraries(e_foc.services.speed_controllers_test PUBLIC
    gmock_main
    e_foc.services.speed_controllers
)

target_sources(e_foc.services.speed_controllers_test PRIVATE
    TestAdrcSpeedController.cpp
    TestLqiSpeedController.cpp
    TestPidSpeedController.cpp
    TestSpeedControllerSelector.cpp
    TestTwoDofSpeedController.cpp
)
