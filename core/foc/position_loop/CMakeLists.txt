add_library(e_foc.foc.position_loop ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.position_loop PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.position_loop PUBLIC
    e_foc.foc.interfaces
    e_foc.foc.selection
    e_foc.foc.speed_loop
    numerical.controllers.implementations
    numerical.filters.passive
    numerical.math
)

target_sources(e_foc.foc.position_loop PRIVATE
    CascadePPositionController.cpp
    CascadePPositionController.hpp
    LqiPositionController.cpp
    LqiPositionController.hpp
    LqrPositionController.cpp
    LqrPositionController.hpp
    PidPositionController.cpp
    PidPositionController.hpp
    PositionController.hpp
    PositionControllerSelector.cpp
    PositionControllerSelector.hpp
    PositionPlantModel.cpp
    PositionPlantModel.hpp
    TwoDofPositionController.cpp
    TwoDofPositionController.hpp
)

add_subdirectory(test)
