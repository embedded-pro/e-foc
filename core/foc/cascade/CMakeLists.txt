add_library(e_foc.foc.cascade ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.cascade PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.cascade PUBLIC
    e_foc.foc.current_loop
    e_foc.foc.interfaces
    e_foc.foc.math
    e_foc.foc.speed_loop
    e_foc.foc.position_loop
    e_foc.foc.transforms
)

target_sources(e_foc.foc.cascade PRIVATE
    CascadeWithSpeedLoop.cpp
    CascadeWithSpeedLoop.hpp
    DirectAxisExcitation.cpp
    DirectAxisExcitation.hpp
    EstimatorChannel.cpp
    EstimatorChannel.hpp
    EstimatorWindowAccumulator.cpp
    EstimatorWindowAccumulator.hpp
    MechanicalWindowAccumulator.hpp
    PositionCascade.cpp
    PositionCascade.hpp
    QuadratureVoltageWindow.cpp
    QuadratureVoltageWindow.hpp
    SpeedCascade.cpp
    SpeedCascade.hpp
    SpeedDifferentiator.cpp
    SpeedDifferentiator.hpp
    TorqueCascade.cpp
    TorqueCascade.hpp
    TripleBuffer.hpp
)

add_subdirectory(test)
