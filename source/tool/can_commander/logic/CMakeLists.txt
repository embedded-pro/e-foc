add_library(e_foc.tool.can_commander.logic ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.tool.can_commander.logic PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.tool.can_commander.logic PUBLIC
    e_foc.services.can_protocol.client
    e_foc.tool.can_commander.adapter
)

target_sources(e_foc.tool.can_commander.logic PRIVATE
    CanCommandClient.hpp
    CanCommandClient.cpp
)

if(MSVC)
    target_compile_options(e_foc.tool.can_commander.logic PRIVATE /W4)
else()
    target_compile_options(e_foc.tool.can_commander.logic PRIVATE
        -Wno-error
    )
endif()

add_subdirectory(test)
