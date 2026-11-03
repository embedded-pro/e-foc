add_executable(e_foc.services.firmware_upgrade_test)
emil_build_for(e_foc.services.firmware_upgrade_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.firmware_upgrade_test)

target_link_libraries(e_foc.services.firmware_upgrade_test PUBLIC
    gmock_main
    e_foc.services.firmware_upgrade.pack_header
)

target_sources(e_foc.services.firmware_upgrade_test PRIVATE
    TestPackHeaderCheck.cpp
)
