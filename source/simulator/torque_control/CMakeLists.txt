add_executable(e_foc.simulator.torque_control)
emil_build_for(e_foc.simulator.torque_control HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_link_libraries(e_foc.simulator.torque_control PUBLIC
    e_foc.simulator.model
    e_foc.simulator.view
    e_foc.simulator.view.gui
    e_foc.simulator.headless
    e_foc.foc.instantiations
    infra.event
    args
)

target_sources(e_foc.simulator.torque_control PRIVATE
    Main.cpp
)

target_compile_definitions(e_foc.simulator.torque_control PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(e_foc.simulator.torque_control PRIVATE /W4)
else()
    target_compile_options(e_foc.simulator.torque_control PRIVATE 
        -Wno-error
    )
endif()
