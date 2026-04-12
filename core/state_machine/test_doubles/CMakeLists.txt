add_library(e_foc.state_machine_mock INTERFACE)
emil_build_for(e_foc.state_machine_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.state_machine_mock INTERFACE
    GTest::gmock
    e_foc.state_machine
)

target_sources(e_foc.state_machine_mock INTERFACE
    FaultNotifierMock.hpp
)
