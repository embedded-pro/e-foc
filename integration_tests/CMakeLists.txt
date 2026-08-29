add_subdirectory(software_in_the_loop)
if(NOT EMIL_BUILD_QEMU)
    add_subdirectory(hardware_in_the_loop)
endif()

