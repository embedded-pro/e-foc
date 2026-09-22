add_library(e_foc.integration_tests.steps OBJECT
    AdcSteps.cpp
    CanSteps.cpp
    CommonSteps.cpp
    ControllerMatrixSteps.cpp
    EepromSteps.cpp
    FaultSteps.cpp
    HardfaultRecoverySteps.cpp
    IdentificationSteps.cpp
    MotorControlSteps.cpp
    PlantSteps.cpp
    ResetCauseSteps.cpp
    ResponseSteps.cpp
    StateMachineSteps.cpp
    UartSteps.cpp
    WatchdogSteps.cpp
)

target_link_libraries(e_foc.integration_tests.steps PUBLIC
    e_foc.integration_tests.support
    e_foc.integration_tests.interactor.hardware
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
