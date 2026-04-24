add_library(e_foc.tool.simulator.services ${EMIL_EXCLUDE_FROM_ALL})

set_target_properties(e_foc.tool.simulator.services PROPERTIES
    AUTOMOC ON
)

target_include_directories(e_foc.tool.simulator.services PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.simulator.services PUBLIC
    Qt6::Widgets
    e_foc.tool.simulator.model
    e_foc.foc.implementations
    numerical.estimators.online
)

target_sources(e_foc.tool.simulator.services PRIVATE
    OnlineElectricalRls.hpp
    OnlineElectricalRls.cpp
    OnlineMechanicalRls.hpp
    OnlineMechanicalRls.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.services PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator.services PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
