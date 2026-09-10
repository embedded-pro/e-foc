add_library(e_foc.integration_tests.steps OBJECT
    AdcSteps.cpp
    CanSteps.cpp
    CommonSteps.cpp
    EepromSteps.cpp
    HardfaultRecoverySteps.cpp
    MotorControlSteps.cpp
    ResetCauseSteps.cpp
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
