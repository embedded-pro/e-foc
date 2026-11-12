add_library(e_foc.services.mechanical_system_ident ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.mechanical_system_ident PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.mechanical_system_ident PUBLIC
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.transforms
    numerical.estimators.online
)

target_sources(e_foc.services.mechanical_system_ident PRIVATE
    EncoderResolution.cpp
    EncoderResolution.hpp
    InertiaSettling.cpp
    InertiaSettling.hpp
    MechanicalEstimatePolicy.hpp
    MechanicalFit.cpp
    MechanicalFit.hpp
    MechanicalWindowKinematics.cpp
    MechanicalWindowKinematics.hpp
    RealTimeFrictionAndInertiaEstimator.cpp
    RealTimeFrictionAndInertiaEstimator.hpp
    MechanicalParametersIdentification.hpp
    MechanicalParametersIdentificationImpl.cpp
    MechanicalParametersIdentificationImpl.hpp
    ObservationQueue.cpp
    ObservationQueue.hpp
    SampleWindow.cpp
    SampleWindow.hpp
    SteadySpeedFrictionFit.hpp
    TwoLevelSpeedProfile.cpp
    TwoLevelSpeedProfile.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
