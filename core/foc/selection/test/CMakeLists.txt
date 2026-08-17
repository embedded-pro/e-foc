add_executable(e_foc.foc.selection_test)
emil_build_for(e_foc.foc.selection_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.selection_test)

target_link_libraries(e_foc.foc.selection_test PUBLIC
    gmock_main
    e_foc.foc.current_loop
    e_foc.foc.speed_loop
)

target_sources(e_foc.foc.selection_test PRIVATE
    TestControllerSelector.cpp
)
