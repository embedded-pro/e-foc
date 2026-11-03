add_library(e_foc.integration_tests.steps OBJECT
    AdcSteps.cpp
    BootloaderSteps.cpp
    CanSteps.cpp
    CommonSteps.cpp
    ControllerMatrixSteps.cpp
    ControllerSettingsSteps.cpp
    CurrentStreamSteps.cpp
    EepromSteps.cpp
    FaultSteps.cpp
    FirmwareIdentitySteps.cpp
    HardfaultRecoverySteps.cpp
    HilMotorSteps.cpp
    IdentificationSteps.cpp
    LoopTimingSteps.cpp
    MotorControlSteps.cpp
    PlantSteps.cpp
    ResetCauseSteps.cpp
    ResponseSteps.cpp
    StateMachineSteps.cpp
    UartSteps.cpp
    WatchdogSteps.cpp
)

target_link_libraries(e_foc.integration_tests.steps PUBLIC
    application.bootloader_image_lib
    bootloader
    services.synchronous_flash
    e_foc.integration_tests.support
    e_foc.integration_tests.interactor.hardware
    cucumber_cpp.runner
    GTest::gtest
    GTest::gmock
)

target_compile_definitions(e_foc.integration_tests.steps PRIVATE
    "E_FOC_EMULATED_PRODUCT_NAME=\"${E_FOC_EMULATED_PRODUCT_NAME}\""
)

add_version_header_dependency(e_foc.integration_tests.steps e_foc.version_header)
