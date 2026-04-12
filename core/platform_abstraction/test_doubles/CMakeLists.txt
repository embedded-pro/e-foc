add_library(e_foc.platform_abstraction_mock INTERFACE)
emil_build_for(e_foc.platform_abstraction_mock HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.platform_abstraction_mock INTERFACE
    gmock_main
    e_foc.platform_abstraction
    hal.synchronous_interfaces
)

target_sources(e_foc.platform_abstraction_mock INTERFACE
    AdcPhaseCurrentMeasurementMock.hpp
    QuadratureEncoderDecoratorMock.hpp
    SynchronousThreeChannelsPwmMock.hpp
)
