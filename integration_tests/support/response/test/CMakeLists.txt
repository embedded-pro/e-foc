add_executable(e_foc.integration_tests.response_test)
emil_build_for(e_foc.integration_tests.response_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.integration_tests.response_test)

target_link_libraries(e_foc.integration_tests.response_test PUBLIC
    gmock_main
    e_foc.integration_tests.response
)

target_sources(e_foc.integration_tests.response_test PRIVATE
    TestPlantTrace.cpp
    TestResponseWindow.cpp
    TestStepMetrics.cpp
)
