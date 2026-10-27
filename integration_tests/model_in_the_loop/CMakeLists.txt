add_library(e_foc.model_in_the_loop STATIC
    ModelInTheLoop.cpp
    ModelInTheLoop.hpp
    SimulatedDrive.cpp
    SimulatedDrive.hpp
)

target_include_directories(e_foc.model_in_the_loop PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
)

target_link_libraries(e_foc.model_in_the_loop PUBLIC
    e_foc.foc.cascade
    e_foc.foc.model
    e_foc.state_machine
    e_foc.services.alignment
    e_foc.services.current_offsets
    e_foc.services.inverter_compensation
    e_foc.services.electrical_system_ident
    e_foc.services.non_volatile_memory
    e_foc.services.standstill
    e_foc.integration_tests.plant
    e_foc.integration_tests.response
    numerical.controllers.implementations
    infra.timer_test_helper
    gmock
)

add_subdirectory(test)
