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
    e_foc.integration_tests.interactor.virtual_motor
    e_foc.integration_tests.interactor.hardware
    cucumber_cpp.runner
)

set(QEMU_SIL_ELF_DEFAULT "${CMAKE_SOURCE_DIR}/build/qemu-foc-sensored/targets/sync_foc_sensored/main/RelWithDebInfo/e_foc.sync_foc_sensored.main.elf")
set(QEMU_SIL_BOOTLOADER_ELF_DEFAULT "${CMAKE_SOURCE_DIR}/build/qemu-foc-sensored/targets/bootloader/main/RelWithDebInfo/e_foc.bootloader.main.elf")
target_compile_definitions(e_foc.integration_tests PRIVATE
    "QEMU_SIL_ELF_DEFAULT_PATH=\"${QEMU_SIL_ELF_DEFAULT}\""
    "QEMU_SIL_BOOTLOADER_ELF_DEFAULT_PATH=\"${QEMU_SIL_BOOTLOADER_ELF_DEFAULT}\""
    "E_FOC_DEVELOPMENT_KEYS_DEFAULT_PATH=\"${CMAKE_SOURCE_DIR}/targets/bootloader/keys/development.keys\""
    "E_FOC_VIRTUAL_MOTOR_DEFAULT_PATH=\"${CMAKE_BINARY_DIR}/tools/virtual_motor/engine/$<CONFIG>/e-foc-virtual-motor\""
)

add_test(NAME e_foc.integration_tests.sil
    COMMAND e_foc.integration_tests --mode sil ${CMAKE_CURRENT_SOURCE_DIR}/../features -t "@sil")
set_tests_properties(e_foc.integration_tests.sil PROPERTIES LABELS integration)

add_test(NAME e_foc.integration_tests.virtual
    COMMAND e_foc.integration_tests --mode virtual ${CMAKE_CURRENT_SOURCE_DIR}/../features -t "@virtual")
set_tests_properties(e_foc.integration_tests.virtual PROPERTIES LABELS integration)
