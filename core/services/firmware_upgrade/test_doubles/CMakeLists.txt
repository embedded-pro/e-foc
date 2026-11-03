add_library(e_foc.services.firmware_upgrade_mock INTERFACE)
emil_build_for(e_foc.services.firmware_upgrade_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.firmware_upgrade_mock INTERFACE
    gmock_main
    e_foc.services.firmware_upgrade
)

target_sources(e_foc.services.firmware_upgrade_mock INTERFACE
    FirmwareUpgradeControlMock.hpp
)
