add_executable(e_foc.foc.model_test)
emil_build_for(e_foc.foc.model_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.model_test)

target_link_libraries(e_foc.foc.model_test PUBLIC
    gmock_main
    e_foc.foc.model
    infra.event_test_helper
)

target_sources(e_foc.foc.model_test PRIVATE
    TestAdcNoise.cpp
    TestEncoderNoise.cpp
    TestThermalModel.cpp
)
