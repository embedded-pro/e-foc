add_library(e_foc.integration_tests.support OBJECT
    EepromStub.cpp
    EepromStub.hpp
    FocIntegrationFixture.cpp
    FocIntegrationFixture.hpp
    FocMotorStateMachineBridge.cpp
    FocMotorStateMachineBridge.hpp
    PlatformFactoryMock.hpp
    PositionIntegrationFixture.cpp
    PositionIntegrationFixture.hpp
    SpeedIntegrationFixture.cpp
    SpeedIntegrationFixture.hpp
    StateMachineAccessor.hpp
)

target_include_directories(e_foc.integration_tests.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.support PUBLIC
    e_foc.state_machine
    e_foc.foc.implementations
    e_foc.platform_abstraction
    e_foc.platform_abstraction_mock
    e_foc.state_machine_mock
    e_foc.services.alignment_mock
    e_foc.services.electrical_system_ident_mock
    e_foc.services.mechanical_system_ident_mock
    e_foc.foc_mock
    can_lite.test_util
    can_lite.categories.foc_motor
    hal.interfaces_test_doubles
    infra.event_test_helper
    infra.util_test_helper
    GTest::gtest
    GTest::gmock
)
