add_executable(e_foc.services.bus_voltage_test)
emil_build_for(e_foc.services.bus_voltage_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.bus_voltage_test)

target_link_libraries(e_foc.services.bus_voltage_test PUBLIC
    gmock_main
    e_foc.services.bus_voltage
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(e_foc.services.bus_voltage_test PRIVATE
    TestBusVoltageMonitor.cpp
)
