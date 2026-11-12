add_executable(e_foc.tool.virtual_motor.app.gui_test)
emil_build_for(e_foc.tool.virtual_motor.app.gui_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.app.gui_test)

target_link_libraries(e_foc.tool.virtual_motor.app.gui_test PUBLIC
    gmock
    e_foc.tool.virtual_motor.app.gui
)

target_sources(e_foc.tool.virtual_motor.app.gui_test PRIVATE
    Main.cpp
    TestManagerWindow.cpp
    TestMotorWindow.cpp
)
