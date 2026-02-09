add_library(e_foc.simulator.plot ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.simulator.plot PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.simulator.plot PUBLIC
    sciplot
)

target_sources(e_foc.simulator.plot PRIVATE
    Plot.hpp
    Plot.cpp
)

if(MSVC)
    target_compile_options(e_foc.simulator.plot PRIVATE /W4)
else()
    target_compile_options(e_foc.simulator.plot PRIVATE 
        -Wno-error
    )
endif()
