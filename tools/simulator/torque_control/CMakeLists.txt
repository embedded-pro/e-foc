add_executable(e_foc.tool.simulator.torque_control)
emil_build_for(e_foc.tool.simulator.torque_control HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_link_libraries(e_foc.tool.simulator.torque_control PUBLIC
    e_foc.tool.simulator.model
    e_foc.tool.simulator.view.gui
    e_foc.tool.simulator.headless
    e_foc.foc.instantiations
    e_foc.services.alignment
    e_foc.services.electrical_system_ident
    hal.generic
    infra.event
    args
)

target_sources(e_foc.tool.simulator.torque_control PRIVATE
    Main.cpp
)

target_compile_definitions(e_foc.tool.simulator.torque_control PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.torque_control PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator.torque_control PRIVATE 
        -Wno-error
    )
endif()
