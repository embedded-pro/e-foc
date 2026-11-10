add_executable(e_foc.tool.virtual_motor.app.logic_test)
emil_build_for(e_foc.tool.virtual_motor.app.logic_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.app.logic_test)

target_link_libraries(e_foc.tool.virtual_motor.app.logic_test PUBLIC
    gmock_main
    e_foc.tool.virtual_motor.app.logic
)

target_sources(e_foc.tool.virtual_motor.app.logic_test PRIVATE
    TestMotorText.cpp
    TestRig.cpp
)
