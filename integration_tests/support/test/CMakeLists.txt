add_executable(e_foc.integration_tests.support_test)
emil_build_for(e_foc.integration_tests.support_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.integration_tests.support_test)

target_link_libraries(e_foc.integration_tests.support_test PUBLIC
    gmock_main
    e_foc.integration_tests.support
    e_foc.integration_tests.interactor.interfaces
)

target_sources(e_foc.integration_tests.support_test PRIVATE
    TestFixture.cpp
)
