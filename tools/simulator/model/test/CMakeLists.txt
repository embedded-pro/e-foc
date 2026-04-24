add_executable(e_foc.tool.simulator.model_test)
emil_build_for(e_foc.tool.simulator.model_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.simulator.model_test)

target_link_libraries(e_foc.tool.simulator.model_test PUBLIC
    gmock_main
    e_foc.tool.simulator.model
    infra.event_test_helper
)

target_sources(e_foc.tool.simulator.model_test PRIVATE
    TestAdcNoise.cpp
    TestEncoderNoise.cpp
    TestThermalModel.cpp
)
