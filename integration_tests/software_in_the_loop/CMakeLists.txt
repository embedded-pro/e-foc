if(EMIL_BUILD_QEMU)
    return()
endif()

add_subdirectory(support)
add_subdirectory(hooks)
add_subdirectory(steps)
add_subdirectory(main)
add_subdirectory(qemu_main)
