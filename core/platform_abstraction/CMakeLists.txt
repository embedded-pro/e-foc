add_library(e_foc.platform_abstraction)
emil_build_for(e_foc.platform_abstraction)

target_include_directories(e_foc.platform_abstraction PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.platform_abstraction PUBLIC
    services.util
    hal.interfaces
    hal.synchronous_interfaces
    infra.util
    infra.event
    e_foc.foc.interfaces
)

target_sources(e_foc.platform_abstraction PRIVATE
    AdcPhaseCurrentMeasurement.hpp
    CanBusAdapter.hpp
    PlatformFactory.hpp
    MotorFieldOrientedControllerAdapter.hpp
    MotorFieldOrientedControllerAdapter.cpp
    QuadratureEncoderDecorator.hpp
)
