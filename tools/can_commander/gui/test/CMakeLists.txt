add_executable(e_foc.tool.can_commander.gui_test)
emil_build_for(e_foc.tool.can_commander.gui_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.can_commander.gui_test)

target_link_libraries(e_foc.tool.can_commander.gui_test PUBLIC
    gmock
    infra.timer_test_helper
    application.bootloader_image_lib
    e_foc.tool.can_commander.gui
)

target_sources(e_foc.tool.can_commander.gui_test PRIVATE
    Main.cpp
    TestDiagnosticsTab.cpp
    TestMotorFirmware.cpp
    TestSettingEditor.cpp
)
