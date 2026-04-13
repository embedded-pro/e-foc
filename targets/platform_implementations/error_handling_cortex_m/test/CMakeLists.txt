add_executable(e_foc.error_handling_cortex_m_test)
emil_build_for(e_foc.error_handling_cortex_m_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.error_handling_cortex_m_test)

target_link_libraries(e_foc.error_handling_cortex_m_test PRIVATE
    gmock_main
    e_foc.error_handling_cortex_m
)

target_sources(e_foc.error_handling_cortex_m_test PRIVATE
    TestPersistentFaultData.cpp
)
