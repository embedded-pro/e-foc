add_subdirectory(interactor/interfaces)
add_subdirectory(interactor/qemu)
add_subdirectory(interactor/hardware)

add_library(e_foc.integration_tests.support OBJECT
    Fixture.hpp
    Fixture.cpp
    nvm/NvmImageBuilder.hpp
    nvm/NvmImageBuilder.cpp
    plant/PlantConfigWriter.hpp
    plant/PlantConfigWriter.cpp
    plant/PlantPresets.hpp
)

target_include_directories(e_foc.integration_tests.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests.support PUBLIC
    e_foc.integration_tests.interactor.interfaces
    e_foc.state_machine
    e_foc.services.non_volatile_memory
    e_foc.can
    hal.interfaces
    can_lite.test_util
    GTest::gtest
    GTest::gmock
)
