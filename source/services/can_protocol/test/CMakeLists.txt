add_executable(e_foc.services.can_protocol_test)
emil_build_for(e_foc.services.can_protocol_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.can_protocol_test)

target_link_libraries(e_foc.services.can_protocol_test PUBLIC
    gmock_main
    e_foc.services.can_protocol
)

target_sources(e_foc.services.can_protocol_test PRIVATE
    CanMock.hpp
    TestCanProtocolHandler.cpp
)
