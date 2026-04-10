add_library(e_foc.services.alignment ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.alignment PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.alignment PUBLIC
    infra.util
    e_foc.platform_abstraction
    e_foc.foc.interfaces
)

target_sources(e_foc.services.alignment PRIVATE
    MotorAlignmentImpl.cpp
    MotorAlignmentImpl.hpp
    MotorAlignment.hpp
    TerminalMotorAlignment.cpp
    TerminalMotorAlignment.hpp
)

add_subdirectory(test)
