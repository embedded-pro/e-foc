add_library(e_foc.tool.virtual_motor.options ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.tool.virtual_motor.options PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
)

target_link_libraries(e_foc.tool.virtual_motor.options PUBLIC
    can_lite.drivers.codec
    can_lite.drivers.virtual
)

target_sources(e_foc.tool.virtual_motor.options PRIVATE
    StartOptions.cpp
    StartOptions.hpp
)

add_executable(e_foc.tool.virtual_motor)
emil_build_for(e_foc.tool.virtual_motor HOST All)

set_target_properties(e_foc.tool.virtual_motor PROPERTIES
    OUTPUT_NAME e-foc-virtual-motor
)

target_link_libraries(e_foc.tool.virtual_motor PRIVATE
    e_foc.tool.virtual_motor.options
    e_foc.tool.virtual_motor.platform
    e_foc.sync_foc_sensored.main.inst
    can_lite.drivers.hub
    numerical.controllers.implementations
)

target_sources(e_foc.tool.virtual_motor PRIVATE
    Main.cpp
)

add_subdirectory(test)
