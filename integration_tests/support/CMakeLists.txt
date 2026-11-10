add_subdirectory(interactor/interfaces)
add_subdirectory(interactor/qemu)
add_subdirectory(interactor/virtual_motor)
add_subdirectory(interactor/hardware)
add_subdirectory(response)

add_library(e_foc.integration_tests.plant STATIC
    nvm/NvmImageBuilder.hpp
    nvm/NvmImageBuilder.cpp
    plant/PlantConfigWriter.hpp
    plant/PlantConfigWriter.cpp
)

target_include_directories(e_foc.integration_tests.plant PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.plant PUBLIC
    e_foc.foc.model
    e_foc.services.non_volatile_memory
    infra.util
)

add_library(e_foc.integration_tests.support OBJECT
    CanParameters.hpp
    CanParameters.cpp
    ControllerSelection.hpp
    ControllerSelection.cpp
    FirmwareUpgradeClient.hpp
    FirmwareUpgradeClient.cpp
    Fixture.hpp
    Fixture.cpp
    TerminalFixture.hpp
    TerminalFixture.cpp
)

target_include_directories(e_foc.integration_tests.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.support PUBLIC
    e_foc.integration_tests.interactor.interfaces
    e_foc.integration_tests.plant
    e_foc.integration_tests.response
    e_foc.state_machine
    e_foc.services.non_volatile_memory
    e_foc.can
    hal.interfaces
    can_lite.categories.firmware_upgrade
    can_lite.test_util
    GTest::gtest
    GTest::gmock
)
