add_library(e_foc.tool.hardware_bridge.terminal ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.hardware_bridge.terminal PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_sources(e_foc.tool.hardware_bridge.terminal PRIVATE
    TerminalTypes.hpp
    TerminalScreen.hpp
    TerminalScreen.cpp
    Vt100Parser.hpp
    Vt100Parser.cpp
    Vt100Terminal.hpp
    Vt100Terminal.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.hardware_bridge.terminal PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.hardware_bridge.terminal PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
