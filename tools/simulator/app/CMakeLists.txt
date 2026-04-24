add_executable(e_foc.tool.simulator)
emil_build_for(e_foc.tool.simulator HOST All PREREQUISITE_BOOL E_FOC_BUILD_SIMULATOR)

target_link_libraries(e_foc.tool.simulator PUBLIC
    Qt6::Widgets
    e_foc.tool.simulator.model
    e_foc.tool.simulator.view.gui
    e_foc.foc.instantiations
    e_foc.services.alignment
    e_foc.services.electrical_system_ident
    e_foc.services.mechanical_system_ident
    e_foc.state_machine
    hal.generic
    infra.event
)

target_sources(e_foc.tool.simulator PRIVATE
    CalibrationsWiring.hpp
    Conversions.hpp
    ControllerKind.hpp
    Defaults.hpp
    LaunchDialog.hpp
    LaunchDialog.cpp
    Main.cpp
    RunController.hpp
    RunPosition.cpp
    RunSpeed.cpp
    RunTorque.cpp
)

set_target_properties(e_foc.tool.simulator PROPERTIES
    AUTOMOC ON
)

target_compile_definitions(e_foc.tool.simulator PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator PRIVATE
        -Wno-error
    )
endif()
