add_executable(e_foc.tool.can_commander.logic_test)
emil_build_for(e_foc.tool.can_commander.logic_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.can_commander.logic_test)

target_link_libraries(e_foc.tool.can_commander.logic_test PUBLIC
    gmock_main
    infra.timer_test_helper
    e_foc.tool.can_commander.logic
    upgrade.staging
)

target_sources(e_foc.tool.can_commander.logic_test PRIVATE
    CanBusAdapterMock.hpp
    CanCommandClientObserverMock.hpp
    TestCanCommandClient.cpp
    TestCommandDescriptions.cpp
    TestFaultHistoryText.cpp
    TestFirmwarePack.cpp
    TestFirmwareRefusal.cpp
    TestFirmwareTransfer.cpp
    TestMotorSettings.cpp
    TestRecoveryAdvice.cpp
    TestSetpointAxis.cpp
    TestPack.cpp
    TestPack.hpp
)
