add_library(e_foc.integration_tests.steps OBJECT
    AdcSteps.cpp
    CalibrationSteps.cpp
    CanSteps.cpp
    CommonSteps.cpp
    CycleBudgetSteps.cpp
    EepromSteps.cpp
    HardfaultRecoverySteps.cpp
    PositionSteps.cpp
    ResetCauseSteps.cpp
    SpeedSteps.cpp
    StateMachineSteps.cpp
    UartSteps.cpp
)

target_link_libraries(e_foc.integration_tests.steps PUBLIC
    e_foc.integration_tests.support
    e_foc.integration_tests.interactor.hardware
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
