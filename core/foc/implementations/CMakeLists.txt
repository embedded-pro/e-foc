add_library(e_foc.foc.implementations ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.implementations PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.implementations PUBLIC
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.transforms
    numerical.controllers.implementations
)

target_sources(e_foc.foc.implementations PRIVATE
    FocPositionImpl.cpp
    FocPositionImpl.hpp
    FocSpeedImpl.cpp
    FocSpeedImpl.hpp
    FocTorqueImpl.cpp
    FocTorqueImpl.hpp
    FocWithSpeedLoop.cpp
    FocWithSpeedLoop.hpp
    LowPriorityInterruptImpl.hpp
    Runner.cpp
    Runner.hpp
    WithAutomaticCurrentPidGains.cpp
    WithAutomaticCurrentPidGains.hpp
    WithAutomaticSpeedPidGains.cpp
    WithAutomaticSpeedPidGains.hpp
)

add_subdirectory(test)
