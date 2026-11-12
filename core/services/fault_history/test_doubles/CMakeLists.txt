add_library(e_foc.services.fault_history_mock INTERFACE)
emil_build_for(e_foc.services.fault_history_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.services.fault_history_mock INTERFACE
    gmock_main
    e_foc.services.fault_history
)

target_sources(e_foc.services.fault_history_mock INTERFACE
    FaultLogMock.hpp
)
