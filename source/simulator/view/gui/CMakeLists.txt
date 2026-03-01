add_library(e_foc.simulator.view.gui ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.simulator.view.gui PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.simulator.view.gui PUBLIC
    Qt6::Widgets
)

target_sources(e_foc.simulator.view.gui PRIVATE
    Gui.hpp
    Gui.cpp
    ScopeWidget.hpp
    ScopeWidget.cpp
    ScopeToolbar.hpp
    ScopeToolbar.cpp
)

set_target_properties(e_foc.simulator.view.gui PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.simulator.view.gui PRIVATE /W4)
else()
    target_compile_options(e_foc.simulator.view.gui PRIVATE
        -Wno-error
    )
endif()
