add_executable(e_foc.tool.hardware_bridge.terminal_test)
emil_build_for(e_foc.tool.hardware_bridge.terminal_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.hardware_bridge.terminal_test)

target_link_libraries(e_foc.tool.hardware_bridge.terminal_test PUBLIC
    gmock_main
    e_foc.tool.hardware_bridge.terminal
)

target_sources(e_foc.tool.hardware_bridge.terminal_test PRIVATE
    TestVt100Parser.cpp
    TestTerminalScreen.cpp
    TestVt100Terminal.cpp
)
