add_executable(e_foc.tool.virtual_motor.platform_test)
emil_build_for(e_foc.tool.virtual_motor.platform_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.platform_test)

target_link_libraries(e_foc.tool.virtual_motor.platform_test PUBLIC
    gmock_main
    infra.event_test_helper
    infra.timer_test_helper
    can_lite.drivers.hub
    e_foc.tool.virtual_motor.platform
)

target_sources(e_foc.tool.virtual_motor.platform_test PRIVATE
    TestFileEeprom.cpp
    TestPacer.cpp
    TestPlantDescription.cpp
    TestPlantDisturbances.cpp
    TestPlantPortClient.cpp
    TestRunLoop.cpp
    TestVirtualCanPort.cpp
)

add_executable(e_foc.tool.virtual_motor.board_test)
emil_build_for(e_foc.tool.virtual_motor.board_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.board_test)

target_link_libraries(e_foc.tool.virtual_motor.board_test PUBLIC
    gtest_main
    e_foc.tool.virtual_motor.platform
)

target_sources(e_foc.tool.virtual_motor.board_test PRIVATE
    TestPlatformPortTarget.cpp
)
