add_executable(e_foc.sync_foc_sensored.main)

add_subdirectory(instantiations)

target_include_directories(e_foc.sync_foc_sensored.main PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.sync_foc_sensored.main PUBLIC
    e_foc.platform_abstraction_impl
    e_foc.sync_foc_sensored.main.inst
    numerical.controllers.implementations
)

target_sources(e_foc.sync_foc_sensored.main PRIVATE
    Main.cpp
)

if(TARGET_MCU_VENDOR STREQUAL "st")
    e_foc_st_target_linker_scripts(e_foc.sync_foc_sensored.main)
    halst_target_default_init(e_foc.sync_foc_sensored.main)
elseif(TARGET_MCU_VENDOR STREQUAL "ti")
    e_foc_ti_target_linker_scripts(e_foc.sync_foc_sensored.main)
    hal_ti_target_default_init(e_foc.sync_foc_sensored.main)
endif()

if(DEFINED TARGET_MCU_VENDOR)
    emil_generate_artifacts(TARGET e_foc.sync_foc_sensored.main MAP BIN HEX)
endif()
