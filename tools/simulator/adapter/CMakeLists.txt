add_library(e_foc.tool.simulator.adapter ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.simulator.adapter PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.simulator.adapter PUBLIC
    Qt6::Core
    e_foc.foc.model
    e_foc.foc.interfaces
    e_foc.services.electrical_system_ident
    e_foc.services.mechanical_system_ident
)

target_sources(e_foc.tool.simulator.adapter PRIVATE
    OnlineElectricalRls.hpp
    OnlineElectricalRls.cpp
    OnlineMechanicalRls.hpp
    OnlineMechanicalRls.cpp
)

set_target_properties(e_foc.tool.simulator.adapter PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.adapter PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator.adapter PRIVATE
        -Wno-error
    )
endif()
