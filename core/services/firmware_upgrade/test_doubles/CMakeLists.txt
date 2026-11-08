add_library(e_foc.services.firmware_upgrade_mock INTERFACE)
emil_build_for(e_foc.services.firmware_upgrade_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.firmware_upgrade_mock INTERFACE
    gmock_main
    e_foc.services.firmware_upgrade.interfaces
)

target_sources(e_foc.services.firmware_upgrade_mock INTERFACE
    FirmwareUpgradeControlMock.hpp
)

add_library(e_foc.services.firmware_upgrade.test_pack STATIC)
emil_build_for(e_foc.services.firmware_upgrade.test_pack HOST All BOOL E_FOC_BUILD_TESTS)

target_include_directories(e_foc.services.firmware_upgrade.test_pack PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
)

target_link_libraries(e_foc.services.firmware_upgrade.test_pack PUBLIC
    upgrade.staging
)

target_sources(e_foc.services.firmware_upgrade.test_pack PRIVATE
    TestPack.hpp
    TestPack.cpp
)
