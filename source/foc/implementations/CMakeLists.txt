add_library(e_foc.foc.implementations ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.implementations PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.implementations PUBLIC
    e_foc.foc.interfaces
    numerical.controllers.implementations
)

target_sources(e_foc.foc.implementations PRIVATE
    FocHelpers.hpp
    FocPositionImpl.cpp
    FocPositionImpl.hpp
    FocSpeedImpl.cpp
    FocSpeedImpl.hpp
    FocTorqueImpl.cpp
    FocTorqueImpl.hpp
    LowPriorityInterruptImpl.hpp
    NormalizedAngles.hpp
    Runner.cpp
    Runner.hpp
    SpaceVectorModulation.cpp
    SpaceVectorModulation.hpp
    TrigonometricImpl.cpp
    TrigonometricImpl.hpp
    WithAutomaticCurrentPidGains.cpp
    WithAutomaticCurrentPidGains.hpp
    TransformsClarkePark.cpp
    TransformsClarkePark.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
