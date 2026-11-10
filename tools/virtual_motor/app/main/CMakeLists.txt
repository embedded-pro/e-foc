add_executable(e_foc.tool.virtual_motor.app)
emil_build_for(e_foc.tool.virtual_motor.app HOST All)

target_link_libraries(e_foc.tool.virtual_motor.app PRIVATE
    hal.generic
    can_lite.drivers.hub
    e_foc.tool.can_commander.support
    e_foc.tool.virtual_motor.app.gui
)

target_sources(e_foc.tool.virtual_motor.app PRIVATE
    Main.cpp
    ../resources/virtual_motor.qrc
)

target_compile_definitions(e_foc.tool.virtual_motor.app PRIVATE
    E_FOC_VIRTUAL_MOTOR_ENGINE="$<TARGET_FILE:e_foc.tool.virtual_motor>"
    E_FOC_CAN_COMMANDER="$<TARGET_FILE:e_foc.tool.can_commander>"
)

set_target_properties(e_foc.tool.virtual_motor.app PROPERTIES
    OUTPUT_NAME e-foc-virtual-motor-app
    AUTORCC ON
    WIN32_EXECUTABLE ON
)

add_dependencies(e_foc.tool.virtual_motor.app e_foc.tool.virtual_motor)

if(MSVC)
    target_compile_options(e_foc.tool.virtual_motor.app PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.virtual_motor.app PRIVATE -Wno-error)
endif()

if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    set(VIRTUAL_MOTOR_ICON "${CMAKE_CURRENT_LIST_DIR}/../resources/e-foc-virtual-motor.ico")
    configure_file(../resources/virtual_motor.rc.in virtual_motor.rc @ONLY)
    target_sources(e_foc.tool.virtual_motor.app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/virtual_motor.rc")
endif()

include(GNUInstallDirs)
install(TARGETS e_foc.tool.virtual_motor.app e_foc.tool.virtual_motor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT can_commander
    EXCLUDE_FROM_ALL
)

if(WIN32)
    if(NOT CMAKE_CROSSCOMPILING)
        qt_generate_deploy_app_script(
            TARGET e_foc.tool.virtual_motor.app
            OUTPUT_SCRIPT deploy_script
            NO_TRANSLATIONS
        )
        install(SCRIPT "${deploy_script}" COMPONENT can_commander EXCLUDE_FROM_ALL)
    endif()
else()
    set(resources "${CMAKE_CURRENT_LIST_DIR}/../resources")
    install(FILES "${resources}/e-foc-virtual-motor.desktop" DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT can_commander EXCLUDE_FROM_ALL)
    install(FILES "${resources}/e-foc-virtual-motor.svg" DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT can_commander EXCLUDE_FROM_ALL)
    install(FILES "${resources}/e-foc-virtual-motor.png" DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps COMPONENT can_commander EXCLUDE_FROM_ALL)
endif()
