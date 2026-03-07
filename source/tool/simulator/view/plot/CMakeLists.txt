add_library(e_foc.tool.simulator.view ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.simulator.view PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.simulator.view PUBLIC
    sciplot
    e_foc.tool.simulator.model
)

target_sources(e_foc.tool.simulator.view PRIVATE
    Plot.hpp
    Plot.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.simulator.view PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.simulator.view PRIVATE
        -Wno-error
    )
endif()
