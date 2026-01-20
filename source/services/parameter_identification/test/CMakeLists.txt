add_executable(e_foc.services.parameter_identification_test)
emil_build_for(e_foc.services.parameter_identification_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.parameter_identification_test)

target_link_libraries(e_foc.services.parameter_identification_test PUBLIC
    gmock_main
    e_foc.services.parameter_identification
    e_foc.foc.implementations
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
    e_foc.foc_mock
)

target_sources(e_foc.services.parameter_identification_test PRIVATE
    TestElectricalParametersIdentification.cpp
    TestTerminalElectricalParametersIdentification.cpp
    TestMechanicalParametersIdentification.cpp
    TestTerminalMechanicalParametersIdentification.cpp
)
