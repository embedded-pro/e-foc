add_executable(e_foc.tool.can_commander)
emil_build_for(e_foc.tool.can_commander HOST All)

target_include_directories(e_foc.tool.can_commander PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../..>"
)

target_link_libraries(e_foc.tool.can_commander PRIVATE
    hal.generic
    e_foc.tool.can_commander.adapter
    e_foc.tool.can_commander.logic
    e_foc.tool.can_commander.support
    e_foc.tool.can_commander.gui
)

target_sources(e_foc.tool.can_commander PRIVATE
    Main.cpp
    ../resources/can_commander.qrc
)

# Installed, packaged and launched as e-foc-can-commander; the target name stays in the e_foc.tool namespace.
set_target_properties(e_foc.tool.can_commander PROPERTIES
    OUTPUT_NAME e-foc-can-commander
    AUTORCC ON
    WIN32_EXECUTABLE ON
)

if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    set(CAN_COMMANDER_ICON "${CMAKE_CURRENT_LIST_DIR}/../resources/e-foc-can-commander.ico")
    configure_file(../resources/can_commander.rc.in can_commander.rc @ONLY)
    target_sources(e_foc.tool.can_commander PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/can_commander.rc")
endif()

if(MSVC)
    target_compile_options(e_foc.tool.can_commander PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander PRIVATE
        -Wno-error
    )
endif()
