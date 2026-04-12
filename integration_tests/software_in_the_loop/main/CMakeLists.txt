add_executable(e_foc.integration_tests)
emil_build_for(e_foc.integration_tests HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.integration_tests PRIVATE
    e_foc.integration_tests.steps
    e_foc.integration_tests.hooks
    e_foc.integration_tests.support
    cucumber_cpp.runner
)

add_test(NAME e_foc.integration_tests
    COMMAND e_foc.integration_tests -- ${CMAKE_CURRENT_SOURCE_DIR}/../features)
