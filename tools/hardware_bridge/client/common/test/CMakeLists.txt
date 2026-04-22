add_executable(e_foc.tool.hardware_bridge.common_test)
emil_build_for(e_foc.tool.hardware_bridge.common_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.hardware_bridge.common_test)

target_link_libraries(e_foc.tool.hardware_bridge.common_test PUBLIC
    gmock_main
    e_foc.tool.hardware_bridge.common
    services.network.connection_test_doubles
    infra.event_test_helper
)

target_sources(e_foc.tool.hardware_bridge.common_test PRIVATE
    TestParseIPv4.cpp
    TestTcpClientBase.cpp
)
