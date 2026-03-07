add_executable(e_foc.services.can_protocol.server_test)
emil_build_for(e_foc.services.can_protocol.server_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.services.can_protocol.server_test)

target_link_libraries(e_foc.services.can_protocol.server_test PUBLIC
    gmock_main
    e_foc.services.can_protocol.server
)

target_sources(e_foc.services.can_protocol.server_test PRIVATE
    CanMock.hpp
    TestCanProtocolServer.cpp
)
