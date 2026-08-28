add_executable(e_foc.foc.transforms_test)
emil_build_for(e_foc.foc.transforms_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.transforms_test)
e_foc_link_qemu_runtime(e_foc.foc.transforms_test)

target_link_libraries(e_foc.foc.transforms_test PUBLIC
    gmock_main
    e_foc.foc.transforms
    numerical.math_test_helper
)

target_sources(e_foc.foc.transforms_test PRIVATE
    TestSpaceVectorModulation.cpp
    TestTransformsClarkePark.cpp
)
