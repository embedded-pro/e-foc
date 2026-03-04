add_library(e_foc.services.cli ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.cli PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.cli PUBLIC
    infra.util
    e_foc.hardware
    e_foc.foc.interfaces
    services.util
    services.tracer
)

target_sources(e_foc.services.cli PRIVATE
    TerminalBase.cpp
    TerminalBase.hpp
    TerminalHelper.hpp
    TerminalPosition.cpp
    TerminalPosition.hpp
    TerminalSpeed.cpp
    TerminalSpeed.hpp
    TerminalTorque.cpp
    TerminalTorque.hpp
    TerminalWithBanner.cpp
    TerminalWithBanner.hpp
)

add_subdirectory(test)
