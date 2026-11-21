add_library(e_foc.tool.can_commander.support ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.can_commander.support PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.can_commander.support PUBLIC
    Qt6::Core
    Qt6::Network
    infra.event
    ui.backend.qt.update
    ui.backend.qt.update_widgets
)

target_compile_definitions(e_foc.tool.can_commander.support PUBLIC
    E_FOC_VERSION="${e_foc_VERSION}"
)

target_sources(e_foc.tool.can_commander.support PRIVATE
    ApplicationInfo.hpp
    QtEventDispatcher.hpp
    QtEventDispatcher.cpp
    Update.hpp
    Update.cpp
)

set_target_properties(e_foc.tool.can_commander.support PROPERTIES
    AUTOMOC ON
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander.support PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander.support PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
