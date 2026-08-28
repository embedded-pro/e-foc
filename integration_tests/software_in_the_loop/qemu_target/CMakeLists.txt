if(NOT EMIL_BUILD_QEMU)
    return()
endif()

add_executable(e_foc.sil_qemu)
e_foc_link_qemu_runtime(e_foc.sil_qemu)
emil_add_test(e_foc.sil_qemu)

target_link_libraries(e_foc.sil_qemu PRIVATE
    e_foc.foc.cascade
    hal.cortex_m
)

target_sources(e_foc.sil_qemu PRIVATE
    SilQemuMain.cpp
)
