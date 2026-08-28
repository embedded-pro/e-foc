function(e_foc_link_qemu_runtime target)
    if(NOT EMIL_BUILD_QEMU)
        return()
    endif()
    target_link_libraries(${target} PRIVATE
        hal.cortex_m
        hal.cortex_m.runtime
        hal.qemu.syscalls
        hal.qemu.default_init
        hal.qemu.sync
        hal.qemu.cortex
    )
endfunction()
