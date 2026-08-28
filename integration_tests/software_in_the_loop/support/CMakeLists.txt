add_library(e_foc.integration_tests.support OBJECT
    ControlModeCoordinationFixture.cpp
    ControlModeCoordinationFixture.hpp
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

if(UNIX)
    target_sources(e_foc.integration_tests.support PRIVATE
        QemuSilFixture.cpp
        QemuSilFixture.hpp
        QemuSilSession.cpp
        QemuSilSession.hpp
    )
endif()

target_include_directories(e_foc.integration_tests.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.support PUBLIC
    e_foc.state_machine
    e_foc.can
    e_foc.foc.cascade
    e_foc.platform_abstraction
    e_foc.platform_abstraction_mock
    e_foc.state_machine_mock
    e_foc.services.alignment_mock
    e_foc.services.electrical_system_ident_mock
    e_foc.services.mechanical_system_ident_mock
    e_foc.foc_mock
    can_lite.test_util
    hal.interfaces_test_doubles
    infra.event_test_helper
    infra.util_test_helper
    GTest::gtest
    GTest::gmock
)

set(QEMU_SIL_ELF_DEFAULT "${CMAKE_SOURCE_DIR}/build/qemu-cortex-m4/integration_tests/software_in_the_loop/qemu_target/RelWithDebInfo/e_foc.sil_qemu.elf")
target_compile_definitions(e_foc.integration_tests.support PUBLIC
    "QEMU_SIL_ELF_DEFAULT_PATH=\"${QEMU_SIL_ELF_DEFAULT}\""
)
