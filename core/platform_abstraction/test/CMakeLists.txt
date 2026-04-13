add_executable(e_foc.platform_abstraction_test)
emil_build_for(e_foc.platform_abstraction_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.platform_abstraction_test)

target_link_libraries(e_foc.platform_abstraction_test PUBLIC
    gmock_main
    e_foc.platform_abstraction
)

target_sources(e_foc.platform_abstraction_test PRIVATE
    TestResetCause.cpp
)
