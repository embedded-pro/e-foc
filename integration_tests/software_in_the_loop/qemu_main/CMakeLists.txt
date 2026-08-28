add_executable(e_foc.qemu_sil_tests)
emil_build_for(e_foc.qemu_sil_tests HOST All BOOL E_FOC_BUILD_TESTS)

target_sources(e_foc.qemu_sil_tests PRIVATE
    QemuSilHooks.cpp
)

target_link_libraries(e_foc.qemu_sil_tests PRIVATE
    e_foc.integration_tests.qemu_steps
    e_foc.integration_tests.support
    cucumber_cpp.runner
)

