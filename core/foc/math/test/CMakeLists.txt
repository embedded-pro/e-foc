add_executable(e_foc.foc.math_test)
emil_build_for(e_foc.foc.math_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.math_test)
e_foc_link_qemu_runtime(e_foc.foc.math_test)

target_link_libraries(e_foc.foc.math_test PUBLIC
    gmock_main
    e_foc.foc.math
)

target_sources(e_foc.foc.math_test PRIVATE
    TestAngleWrap.cpp
    TestFastTrigonometry.cpp
)
