add_library(e_foc.drivers_mock INTERFACE)
emil_build_for(e_foc.drivers_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.drivers_mock INTERFACE
    gmock_main
    e_foc.platform_abstraction.interfaces
)

target_sources(e_foc.drivers_mock INTERFACE
    DriversMock.hpp
)
