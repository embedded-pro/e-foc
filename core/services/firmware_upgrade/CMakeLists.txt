add_library(e_foc.services.firmware_upgrade ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.firmware_upgrade PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.firmware_upgrade PUBLIC
    bootloader
    can_lite.categories.firmware_upgrade
    e_foc.platform_abstraction
    hal.interfaces
    hal.synchronous_interfaces
    infra.timer
    infra.util
    upgrade.pack
)

target_sources(e_foc.services.firmware_upgrade PRIVATE
    FirmwareUpgrade.cpp
    FirmwareUpgrade.hpp
    FirmwareUpgradeControl.hpp
    FirmwareUpgradeDrive.hpp
    PackHeaderCheck.cpp
    PackHeaderCheck.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
