add_library(e_foc.tool.simulator.headless ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.simulator.headless PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.simulator.headless PUBLIC
    e_foc.tool.simulator.model
    e_foc.foc.interfaces
    infra.event
)

target_sources(e_foc.tool.simulator.headless PRIVATE
    HeadlessSimulation.hpp
    HeadlessSimulation.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.headless PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator.headless PRIVATE
        -Wno-error
    )
endif()
