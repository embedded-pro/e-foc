add_library(e_foc.services.position_tracking ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.position_tracking PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.position_tracking PUBLIC
    infra.timer
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.cascade
)

target_sources(e_foc.services.position_tracking PRIVATE
    PositionTracker.cpp
    PositionTracker.hpp
)

add_subdirectory(test)
