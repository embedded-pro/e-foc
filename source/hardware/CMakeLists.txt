add_library(e_foc.hardware)
emil_build_for(e_foc.hardware)

target_include_directories(e_foc.hardware PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.hardware PUBLIC
    services.util
    hal.interfaces
    hal.synchronous_interfaces
    infra.util
    infra.event
    e_foc.foc.interfaces
)

target_sources(e_foc.hardware PRIVATE
    AdcPhaseCurrentMeasurement.hpp
    CanBusAdapter.hpp
    HardwareFactory.hpp
    MotorFieldOrientedControllerAdapter.hpp
    MotorFieldOrientedControllerAdapter.cpp
    QuadratureEncoderDecorator.hpp
)

add_subdirectory(${E_FOC_TARGET_PLATFORM})

add_library(e_foc.hardware_impl INTERFACE)

target_compile_definitions(e_foc.hardware_impl INTERFACE
    HARDWARE_FACTORY_IMPL_HEADER="source/hardware/${E_FOC_TARGET_PLATFORM}/implementation/HardwareFactoryImpl.hpp"
)

target_link_libraries(e_foc.hardware_impl INTERFACE
    e_foc.hardware_${E_FOC_TARGET_PLATFORM}
)
