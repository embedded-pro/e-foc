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

add_test(NAME e_foc.qemu_sil_tests
    COMMAND e_foc.qemu_sil_tests -- ${CMAKE_CURRENT_SOURCE_DIR}/../features)
set_tests_properties(e_foc.qemu_sil_tests PROPERTIES LABELS integration)
