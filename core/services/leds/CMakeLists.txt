add_library(e_foc.services.leds ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.leds PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.leds PUBLIC
    hal.interfaces
    infra.timer
    infra.util
)

target_sources(e_foc.services.leds PRIVATE
    LedPattern.cpp
    LedPattern.hpp
    LedStatusSource.hpp
    StatusLeds.cpp
    StatusLeds.hpp
)

add_subdirectory(test)
