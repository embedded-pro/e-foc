add_executable(e_foc.tool.virtual_motor.app)
emil_build_for(e_foc.tool.virtual_motor.app HOST All)

target_link_libraries(e_foc.tool.virtual_motor.app PRIVATE
    hal.generic
    can_lite.drivers.hub
    e_foc.tool.can_commander.support
    e_foc.tool.virtual_motor.app.gui
)

target_sources(e_foc.tool.virtual_motor.app PRIVATE
    Main.cpp
)

target_compile_definitions(e_foc.tool.virtual_motor.app PRIVATE
    E_FOC_VIRTUAL_MOTOR_ENGINE="$<TARGET_FILE:e_foc.tool.virtual_motor>"
    E_FOC_CAN_COMMANDER="$<TARGET_FILE:e_foc.tool.can_commander>"
)

set_target_properties(e_foc.tool.virtual_motor.app PROPERTIES
    OUTPUT_NAME e-foc-virtual-motor-app
    WIN32_EXECUTABLE ON
)

add_dependencies(e_foc.tool.virtual_motor.app e_foc.tool.virtual_motor)

if(MSVC)
    target_compile_options(e_foc.tool.virtual_motor.app PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.virtual_motor.app PRIVATE -Wno-error)
endif()
