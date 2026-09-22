add_executable(e_foc.services.injection_current_limit_test)
emil_build_for(e_foc.services.injection_current_limit_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.injection_current_limit_test)

target_link_libraries(e_foc.services.injection_current_limit_test PUBLIC
    gmock_main
    e_foc.services.injection_current_limit
)

target_sources(e_foc.services.injection_current_limit_test PRIVATE
    TestInjectionCurrentLimit.cpp
)
