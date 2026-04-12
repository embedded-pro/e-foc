add_executable(e_foc.hil_tests)

# HIL tests are NOT registered with ctest and NOT built by default.
# They require a connected hardware target.
# Build manually with: cmake --build --preset host-Debug --target e_foc.hil_tests
emil_build_for(e_foc.hil_tests HOST All BOOL E_FOC_BUILD_HIL_TESTS)

target_link_libraries(e_foc.hil_tests PRIVATE
    e_foc.hil_tests.steps
    e_foc.hil_tests.hooks
    e_foc.hil_tests.support
    cucumber_cpp.runner
)

# HIL tests are intentionally NOT added to ctest.
# Run manually: ./e_foc.hil_tests -- /path/to/hil/features
