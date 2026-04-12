add_library(e_foc.integration_tests.steps OBJECT
    CalibrationSteps.cpp
    CanSteps.cpp
    PositionSteps.cpp
    SpeedSteps.cpp
    StateMachineSteps.cpp
)

target_link_libraries(e_foc.integration_tests.steps PUBLIC
    e_foc.integration_tests.support
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
