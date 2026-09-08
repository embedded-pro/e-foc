add_executable(e_foc.integration_tests)
emil_build_for(e_foc.integration_tests HOST All BOOL E_FOC_BUILD_TESTS)

target_sources(e_foc.integration_tests PRIVATE
    Main.cpp
)

target_include_directories(e_foc.integration_tests PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.integration_tests PRIVATE
    e_foc.integration_tests.steps
    e_foc.integration_tests.hooks
    e_foc.integration_tests.support
    e_foc.integration_tests.interactor.interfaces
    e_foc.integration_tests.interactor.qemu
    e_foc.integration_tests.interactor.hardware
    cucumber_cpp.runner
)

set(QEMU_SIL_ELF_DEFAULT "${CMAKE_SOURCE_DIR}/build/qemu-foc-sensored/targets/sync_foc_sensored/main/RelWithDebInfo/e_foc.sync_foc_sensored.main.elf")
target_compile_definitions(e_foc.integration_tests PRIVATE
    "QEMU_SIL_ELF_DEFAULT_PATH=\"${QEMU_SIL_ELF_DEFAULT}\""
)

add_test(NAME e_foc.integration_tests.sil
    COMMAND e_foc.integration_tests --mode sil ${CMAKE_CURRENT_SOURCE_DIR}/../features -t "@sil")
set_tests_properties(e_foc.integration_tests.sil PROPERTIES LABELS integration)
