add_executable(e_foc.tool.hardware_bridge.can_client_test)
emil_build_for(e_foc.tool.hardware_bridge.can_client_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.hardware_bridge.can_client_test)

target_link_libraries(e_foc.tool.hardware_bridge.can_client_test PUBLIC
    gmock_main
    e_foc.tool.hardware_bridge.can_client
    services.network.connection_test_doubles
    infra.event_test_helper
)

target_sources(e_foc.tool.hardware_bridge.can_client_test PRIVATE
    TestTcpClientCanbus.cpp
)
