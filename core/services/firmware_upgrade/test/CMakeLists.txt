add_executable(e_foc.services.firmware_upgrade_test)
emil_build_for(e_foc.services.firmware_upgrade_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.firmware_upgrade_test)

target_link_libraries(e_foc.services.firmware_upgrade_test PUBLIC
    gmock_main
    can_lite.test_util
    e_foc.services.firmware_upgrade
    e_foc.services.firmware_upgrade.test_pack
    hal.interfaces_test_doubles
    hal.synchronous_interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(e_foc.services.firmware_upgrade_test PRIVATE
    TestFirmwareUpgrade.cpp
)
