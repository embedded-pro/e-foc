add_library(e_foc.foc.cascade ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.cascade PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.cascade PUBLIC
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.transforms
    numerical.controllers.implementations
)

target_sources(e_foc.foc.cascade PRIVATE
    PositionCascade.cpp
    PositionCascade.hpp
    SpeedCascade.cpp
    SpeedCascade.hpp
    TorqueCascade.cpp
    TorqueCascade.hpp
    CascadeWithSpeedLoop.cpp
    CascadeWithSpeedLoop.hpp
    WithAutomaticCurrentPidGains.cpp
    WithAutomaticCurrentPidGains.hpp
    WithAutomaticSpeedPidGains.cpp
    WithAutomaticSpeedPidGains.hpp
)

add_subdirectory(test)
