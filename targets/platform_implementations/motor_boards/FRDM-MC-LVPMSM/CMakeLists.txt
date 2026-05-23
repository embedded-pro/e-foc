add_library(e_foc.motor_board INTERFACE)

target_include_directories(e_foc.motor_board INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_definitions(e_foc.motor_board INTERFACE
    MOTOR_BOARD_CHARACTERISTICS_HEADER="targets/platform_implementations/motor_boards/FRDM-MC-LVPMSM/BoardCharacteristics.hpp"
)

target_sources(e_foc.motor_board INTERFACE
    BoardCharacteristics.hpp
)
