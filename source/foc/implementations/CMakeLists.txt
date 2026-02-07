add_library(e_foc.foc.implementations ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.implementations PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.implementations PUBLIC
    e_foc.foc.interfaces
)

target_sources(e_foc.foc.implementations PRIVATE
    ControllerBaseImpl.cpp
    ControllerBaseImpl.hpp
    FocTorqueImpl.cpp
    FocTorqueImpl.hpp
    FocWithSpeedPidImpl.cpp
    FocWithSpeedPidImpl.hpp
    NormalizedAngles.hpp
    RealTimeFrictionAndInertiaEstimator.cpp
    RealTimeFrictionAndInertiaEstimator.hpp
    SpaceVectorModulation.cpp
    SpaceVectorModulation.hpp
    SpeedControllerImpl.cpp
    SpeedControllerImpl.hpp
    TorqueControllerImpl.cpp
    TorqueControllerImpl.hpp
    TransformsClarkePark.cpp
    TransformsClarkePark.hpp
    WithAutomaticCurrentPidGains.cpp
    WithAutomaticCurrentPidGains.hpp
    WithRealTimeSpeedPidTune.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
