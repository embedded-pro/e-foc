add_library(e_foc.foc_mock INTERFACE)
emil_build_for(e_foc.foc_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.foc_mock INTERFACE
    gmock_main
    e_foc.foc.interfaces
    numerical.math_test_helper
    infra.timer_test_helper
)

target_sources(e_foc.foc_mock PRIVATE
    DriversMock.hpp
    FocMock.hpp
)
