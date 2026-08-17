add_executable(e_foc.foc.position_loop_test)
emil_build_for(e_foc.foc.position_loop_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.position_loop_test)

target_link_libraries(e_foc.foc.position_loop_test PUBLIC
    gmock_main
    e_foc.foc.position_loop
)

target_sources(e_foc.foc.position_loop_test PRIVATE
    TestPositionControllerSelector.cpp
    TestTwoDofPositionController.cpp
)
