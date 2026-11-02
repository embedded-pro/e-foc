# The firmware image tool (documentation/design/bootloader.md, Part J) runs on the build host. A host build has it
# as an ordinary target; an embedded build configures and builds it once with the host compiler and imports it.
function(e_foc_firmware_image_tool)
    if(TARGET e_foc.tool.firmware_image)
        return()
    endif()

    include(ExternalProject)
    FetchContent_GetProperties(microecc)

    set(binaryDir "${CMAKE_BINARY_DIR}/_native_firmware_image")
    set(tool "${binaryDir}/e_foc.tool.firmware_image${CMAKE_HOST_EXECUTABLE_SUFFIX}")

    ExternalProject_Add(e_foc_native_firmware_image
        SOURCE_DIR "${PROJECT_SOURCE_DIR}/tools/firmware_image"
        BINARY_DIR "${binaryDir}"
        CMAKE_GENERATOR "Ninja"
        CMAKE_ARGS
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_TOOLCHAIN_FILE="
            "-DFETCHCONTENT_SOURCE_DIR_MICROECC=${microecc_SOURCE_DIR}"
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target e_foc.tool.firmware_image
        BUILD_BYPRODUCTS "${tool}"
        INSTALL_COMMAND ""
        BUILD_ALWAYS On
    )

    add_executable(e_foc.tool.firmware_image IMPORTED GLOBAL)
    set_target_properties(e_foc.tool.firmware_image PROPERTIES IMPORTED_LOCATION "${tool}")
    add_dependencies(e_foc.tool.firmware_image e_foc_native_firmware_image)
endfunction()
