add_library(e_foc.services.speed_controllers ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.speed_controllers PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.speed_controllers PUBLIC
    e_foc.foc.interfaces
    e_foc.services.controller_selection
    numerical.controllers.implementations
    numerical.filters.passive
    numerical.math
    numerical.robust_control
)

target_sources(e_foc.services.speed_controllers PRIVATE
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
