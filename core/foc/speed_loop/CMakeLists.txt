add_library(e_foc.foc.speed_loop ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.speed_loop PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.speed_loop PUBLIC
    e_foc.foc.interfaces
    e_foc.foc.selection
    numerical.controllers.implementations
    numerical.filters.passive
    numerical.math
    numerical.robust_control
)

target_sources(e_foc.foc.speed_loop PRIVATE
    AdrcSpeedController.cpp
    AdrcSpeedController.hpp
    LqiSpeedController.cpp
    LqiSpeedController.hpp
    PidSpeedController.cpp
    PidSpeedController.hpp
    SpeedController.hpp
    SpeedControllerSelector.cpp
    SpeedControllerSelector.hpp
    SpeedPlantModel.cpp
    SpeedPlantModel.hpp
    TwoDofSpeedController.cpp
    TwoDofSpeedController.hpp
)

add_subdirectory(test)
