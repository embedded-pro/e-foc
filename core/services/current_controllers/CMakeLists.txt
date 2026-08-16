add_library(e_foc.services.current_controllers ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.current_controllers PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.current_controllers PUBLIC
    e_foc.foc.interfaces
    e_foc.services.controller_selection
    numerical.controllers.implementations
    numerical.math
    numerical.robust_control
)

target_sources(e_foc.services.current_controllers PRIVATE
    CurrentController.hpp
    CurrentControllerSelector.cpp
    CurrentControllerSelector.hpp
    CurrentPlantModel.cpp
    CurrentPlantModel.hpp
    DeadbeatCurrentController.cpp
    DeadbeatCurrentController.hpp
    DecoupledPidCurrentController.cpp
    DecoupledPidCurrentController.hpp
    PidCurrentController.cpp
    PidCurrentController.hpp
    SlidingModeCurrentController.cpp
    SlidingModeCurrentController.hpp
)

add_subdirectory(test)
