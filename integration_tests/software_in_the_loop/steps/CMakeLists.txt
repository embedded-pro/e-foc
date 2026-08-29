add_library(e_foc.integration_tests.steps OBJECT
    CalibrationSteps.cpp
    CallbackRaceSteps.cpp
    CanSteps.cpp
    ControlModeSteps.cpp
    PositionFunctionalSteps.cpp
    PositionSteps.cpp
    SpeedFunctionalSteps.cpp
    SpeedSteps.cpp
    StateMachineSteps.cpp
)

target_link_libraries(e_foc.integration_tests.steps PUBLIC
    e_foc.integration_tests.support
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)

add_library(e_foc.integration_tests.qemu_steps OBJECT
    FocCycleBudgetSteps.cpp
    QemuCalibrationSteps.cpp
    QemuCanSteps.cpp
    QemuCycleBudgetSteps.cpp
    QemuPositionSteps.cpp
    QemuSpeedSteps.cpp
    QemuStateMachineSteps.cpp
)

target_link_libraries(e_foc.integration_tests.qemu_steps PUBLIC
    e_foc.integration_tests.support
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
