add_library(e_foc.services.standstill ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.standstill PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.standstill PUBLIC
    infra.timer
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.foc.math
)

target_sources(e_foc.services.standstill PRIVATE
    RotorStandstill.hpp
    RotorStandstillImpl.cpp
    RotorStandstillImpl.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
