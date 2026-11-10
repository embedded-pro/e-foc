add_executable(e_foc.tool.virtual_motor.app.logic_test)
emil_build_for(e_foc.tool.virtual_motor.app.logic_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.app.logic_test)

target_link_libraries(e_foc.tool.virtual_motor.app.logic_test PUBLIC
    gmock_main
    e_foc.tool.virtual_motor.app.logic
    ui.backend.recording
)

target_sources(e_foc.tool.virtual_motor.app.logic_test PRIVATE
    TestCaptureAssembly.cpp
    TestCaptureCsv.cpp
    TestEncoderView.cpp
    TestModulationView.cpp
    TestMotorText.cpp
    TestRig.cpp
    TestRotorView.cpp
    TestTraceHistory.cpp
    TestTraceView.cpp
)
