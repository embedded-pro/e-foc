add_executable(e_foc.services.can_protocol.core_test)
emil_build_for(e_foc.services.can_protocol.core_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.can_protocol.core_test)

target_link_libraries(e_foc.services.can_protocol.core_test PRIVATE
    e_foc.services.can_protocol.core
    gtest_main
)

target_sources(e_foc.services.can_protocol.core_test PRIVATE
    TestCanProtocolCore.cpp
)
