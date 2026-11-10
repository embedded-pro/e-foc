add_executable(e_foc.tool.virtual_motor.options_test)
emil_build_for(e_foc.tool.virtual_motor.options_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.tool.virtual_motor.options_test)

target_link_libraries(e_foc.tool.virtual_motor.options_test PUBLIC
    gmock_main
    e_foc.tool.virtual_motor.options
)

target_sources(e_foc.tool.virtual_motor.options_test PRIVATE
    TestStartOptions.cpp
    TestSupervisor.cpp
)

if(UNIX)
    add_executable(e_foc.tool.virtual_motor.engine_test)
    emil_build_for(e_foc.tool.virtual_motor.engine_test HOST All BOOL E_FOC_BUILD_TESTS)
    emil_add_test(e_foc.tool.virtual_motor.engine_test)

    target_link_libraries(e_foc.tool.virtual_motor.engine_test PUBLIC
        gtest_main
        e_foc.tool.virtual_motor.platform
        e_foc.sync_foc_sensored.main.inst
        can_lite.drivers.hub
        numerical.controllers.implementations
    )

    target_sources(e_foc.tool.virtual_motor.engine_test PRIVATE
        TestVirtualMotor.cpp
    )
endif()
