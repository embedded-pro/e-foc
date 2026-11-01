add_library(e_foc.bootloader ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.bootloader PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.bootloader PUBLIC
    infra.util
    hal.synchronous_interfaces
    services.crypto
    upgrade.boot_loader
    upgrade.pack
)

target_sources(e_foc.bootloader PRIVATE
    ApplicationCheck.cpp
    ApplicationCheck.hpp
    ApplicationInstaller.cpp
    ApplicationInstaller.hpp
    BootSequence.cpp
    BootSequence.hpp
    ImageFormat.hpp
    JournalPackStatus.cpp
    JournalPackStatus.hpp
    StatusJournal.cpp
    StatusJournal.hpp
)

add_subdirectory(test)
