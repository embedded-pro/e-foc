add_executable(e_foc.simulator.speed_control)
emil_build_for(e_foc.simulator.speed_control HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_link_libraries(e_foc.simulator.speed_control PUBLIC
    e_foc.simulator.pmsm
    e_foc.simulator.plot
    e_foc.foc.instantiations
)

target_sources(e_foc.simulator.speed_control PRIVATE
    Main.cpp
)

if(MSVC)
    target_compile_options(e_foc.simulator.speed_control PRIVATE /W4)
else()
    target_compile_options(e_foc.simulator.speed_control PRIVATE 
        -Wno-error
    )
endif()
