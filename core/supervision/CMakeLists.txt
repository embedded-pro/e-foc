add_library(e_foc.supervision STATIC)

target_include_directories(e_foc.supervision PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.supervision PUBLIC
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.platform_abstraction
    e_foc.services.non_volatile_memory
    e_foc.state_machine
    infra.timer
    services.tracer
)

target_sources(e_foc.supervision PRIVATE
    ControlLoopStallMonitor.hpp
    ControlLoopStallMonitor.cpp
    EncoderPlausibilityMonitor.hpp
    EncoderPlausibilityMonitor.cpp
)

add_subdirectory(test)
