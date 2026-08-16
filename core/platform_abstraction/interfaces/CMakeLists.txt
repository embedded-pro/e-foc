add_library(e_foc.platform_abstraction.interfaces INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.platform_abstraction.interfaces INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction.interfaces INTERFACE
    e_foc.foc.interfaces
    hal.synchronous_interfaces
    infra.util
)

target_sources(e_foc.platform_abstraction.interfaces INTERFACE
    Drivers.hpp
)

add_subdirectory(test_doubles)
