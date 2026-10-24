include(GNUInstallDirs)

set(component can_commander)
set(resources "${CMAKE_CURRENT_LIST_DIR}/../resources")

# EXCLUDE_FROM_ALL keeps the tool out of the e_foc host install package; only this
# component's own package installs it.
install(TARGETS e_foc.tool.can_commander
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${component}
    EXCLUDE_FROM_ALL
)

if(WIN32)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION . RENAME LICENSE.txt COMPONENT ${component} EXCLUDE_FROM_ALL)

    if(NOT CMAKE_CROSSCOMPILING)
        qt_generate_deploy_app_script(
            TARGET e_foc.tool.can_commander
            OUTPUT_SCRIPT deploy_script
            NO_TRANSLATIONS
        )
        install(SCRIPT "${deploy_script}" COMPONENT ${component} EXCLUDE_FROM_ALL)
    endif()
else()
    install(FILES e-foc-can-commander.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT ${component} EXCLUDE_FROM_ALL)
    install(FILES "${resources}/e-foc-can-commander.svg" DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps COMPONENT ${component} EXCLUDE_FROM_ALL)
    install(FILES "${resources}/e-foc-can-commander.png" DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps COMPONENT ${component} EXCLUDE_FROM_ALL)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/e-foc-can-commander RENAME copyright COMPONENT ${component} EXCLUDE_FROM_ALL)
endif()

# A package of its own, next to the project-wide CPackConfig.cmake:
#   cpack --config <build>/tools/can_commander/installer/CPackConfig.cmake -C Release -G DEB|NSIS
function(e_foc_can_commander_package)
    set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake")
    set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/CPackSourceConfig.cmake")
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
    set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};e_foc;${component};/")
    set(CPACK_MONOLITHIC_INSTALL ON)
    set(CPACK_VERBATIM_VARIABLES ON)

    set(CPACK_PACKAGE_NAME "e-foc-can-commander")
    set(CPACK_PACKAGE_VENDOR "embedded-pro")
    set(CPACK_PACKAGE_VERSION "${e_foc_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "e-foc CAN Commander: command and monitor e-foc motor controllers over CAN")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/embedded-pro/e-foc")
    set(CPACK_PACKAGE_FILE_NAME "e_foc-can_commander-v${e_foc_VERSION}-${CMAKE_SYSTEM_NAME}")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_EXECUTABLES "e-foc-can-commander" "e-foc CAN Commander")
    set(CPACK_STRIP_FILES ON)

    set(CPACK_DEBIAN_PACKAGE_NAME "e-foc-can-commander")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "embedded-pro")
    set(CPACK_DEBIAN_PACKAGE_SECTION "electronics")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Desktop tool to command and monitor e-foc motor controllers over CAN.\n Connects through SocketCAN, keeps an issue log and checks for new releases.")

    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "e-foc CAN Commander")
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "e-foc CAN Commander")
    set(CPACK_NSIS_PACKAGE_NAME "e-foc CAN Commander")
    set(CPACK_NSIS_DISPLAY_NAME "e-foc CAN Commander")
    set(CPACK_NSIS_BRANDING_TEXT "e-foc")
    set(CPACK_NSIS_MUI_ICON "${resources}/e-foc-can-commander.ico")
    set(CPACK_NSIS_MUI_UNIICON "${resources}/e-foc-can-commander.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\e-foc-can-commander.exe")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "e-foc-can-commander.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/embedded-pro/e-foc")
    set(CPACK_NSIS_HELP_LINK "https://github.com/embedded-pro/e-foc/issues")
    # windeployqt leaves the MSVC redistributable beside the executable; run it so the tool starts
    # on machines that lack the runtime.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [=[
  CreateShortCut "$DESKTOP\e-foc CAN Commander.lnk" "$INSTDIR\bin\e-foc-can-commander.exe"
  IfFileExists "$INSTDIR\bin\vc_redist.x64.exe" 0 +3
    ExecWait '"$INSTDIR\bin\vc_redist.x64.exe" /install /quiet /norestart'
    Delete "$INSTDIR\bin\vc_redist.x64.exe"
]=])
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
  Delete "$DESKTOP\e-foc CAN Commander.lnk"
]=])

    include(CPack)
endfunction()

e_foc_can_commander_package()
