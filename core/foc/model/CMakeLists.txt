add_library(e_foc.foc.model STATIC)

target_include_directories(e_foc.foc.model PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
)

target_sources(e_foc.foc.model PRIVATE
    ThreePhaseMotorModel.cpp
    ThreePhaseMotorModel.hpp
    Jk42bls01X038ed.hpp
)

target_link_libraries(e_foc.foc.model PUBLIC
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.transforms
    e_foc.platform_abstraction.interfaces
    hal.synchronous_interfaces
    infra.event
    infra.util
    numerical.math
)
