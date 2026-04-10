add_executable(e_foc.tool.simulator.position_control)
emil_build_for(e_foc.tool.simulator.position_control HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_link_libraries(e_foc.tool.simulator.position_control PUBLIC
    e_foc.tool.simulator.model
    e_foc.tool.simulator.view
    e_foc.tool.simulator.view.gui
    e_foc.tool.simulator.headless
    e_foc.foc.instantiations
    infra.event
    args
)

target_sources(e_foc.tool.simulator.position_control PRIVATE
    Main.cpp
)

target_compile_definitions(e_foc.tool.simulator.position_control PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.position_control PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator.position_control PRIVATE 
        -Wno-error
    )
endif()
