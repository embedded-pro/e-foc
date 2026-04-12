add_library(e_foc.hil_tests.steps OBJECT
    AdcSteps.cpp
    CanSteps.cpp
    CommonSteps.cpp
    EepromSteps.cpp
    EncoderSteps.cpp
    FocCycleBudgetSteps.cpp
    PwmSteps.cpp
    UartSteps.cpp
)

target_link_libraries(e_foc.hil_tests.steps PUBLIC
    e_foc.hil_tests.support
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)
