add_library(e_foc.services.firmware_upgrade.interfaces INTERFACE)

target_include_directories(e_foc.services.firmware_upgrade.interfaces INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(e_foc.services.firmware_upgrade.interfaces INTERFACE
    FirmwareVersion.hpp
)

add_library(e_foc.services.firmware_upgrade.pack_header ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.services.firmware_upgrade.pack_header PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.firmware_upgrade.pack_header PUBLIC
    e_foc.services.firmware_upgrade.interfaces
    infra.util
    upgrade.pack
)

target_sources(e_foc.services.firmware_upgrade.pack_header PRIVATE
    PackHeaderCheck.cpp
    PackHeaderCheck.hpp
)

add_subdirectory(test)
