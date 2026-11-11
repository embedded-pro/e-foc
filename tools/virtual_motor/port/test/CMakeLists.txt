add_executable(e_foc.tool.virtual_motor.port_test)
emil_build_for(e_foc.tool.virtual_motor.port_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.port_test)

target_link_libraries(e_foc.tool.virtual_motor.port_test PUBLIC
    gmock_main
    e_foc.tool.virtual_motor.port
)

target_sources(e_foc.tool.virtual_motor.port_test PRIVATE
    TestPlantPortCodec.cpp
)
