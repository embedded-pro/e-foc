add_executable(e_foc.tool.simulator.services_test)
emil_build_for(e_foc.tool.simulator.services_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.simulator.services_test)

target_link_libraries(e_foc.tool.simulator.services_test PUBLIC
    gmock_main
    e_foc.tool.simulator.services
)

target_sources(e_foc.tool.simulator.services_test PRIVATE
    TestOnlineElectricalRls.cpp
    TestOnlineMechanicalRls.cpp
)
