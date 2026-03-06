add_executable(e_foc.tool.can_commander.logic_test)
emil_build_for(e_foc.tool.can_commander.logic_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.can_commander.logic_test)

target_link_libraries(e_foc.tool.can_commander.logic_test PUBLIC
    gmock_main
    e_foc.tool.can_commander.logic
)

target_sources(e_foc.tool.can_commander.logic_test PRIVATE
    CanBusAdapterMock.hpp
    CanCommandClientObserverMock.hpp
    TestCanCommandClient.cpp
)
