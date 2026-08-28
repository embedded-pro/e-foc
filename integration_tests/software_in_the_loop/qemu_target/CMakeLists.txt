if(NOT EMIL_BUILD_QEMU)
    return()
endif()

add_executable(e_foc.sil_qemu)
e_foc_link_qemu_runtime(e_foc.sil_qemu)

target_link_libraries(e_foc.sil_qemu PRIVATE
    e_foc.foc.cascade
    e_foc.state_machine
    e_foc.can
    e_foc.services.electrical_system_ident
    e_foc.services.alignment
    e_foc.services.non_volatile_memory
    can_lite.server
    hal.cortex_m
)

target_sources(e_foc.sil_qemu PRIVATE
    SemihostingCan.cpp
    SilQemuApplication.cpp
    SilQemuMain.cpp
)

add_test(NAME e_foc.sil_qemu COMMAND e_foc.sil_qemu --benchmark)
