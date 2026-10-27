add_library(e_foc.services.inverter_compensation ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.inverter_compensation PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.inverter_compensation PUBLIC
    infra.util
    e_foc.platform_abstraction.interfaces
    e_foc.foc.interfaces
    numerical.math
)

target_sources(e_foc.services.inverter_compensation PRIVATE
    InverterCompensation.cpp
    InverterCompensation.hpp
)

add_subdirectory(test)
