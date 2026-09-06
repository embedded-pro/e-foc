add_library(e_foc.foc.math INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.math INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.math INTERFACE
    e_foc.foc.interfaces
    numerical.math
)

target_sources(e_foc.foc.math INTERFACE
    AngleWrap.hpp
    DutyConversion.hpp
    FastTrigonometry.hpp
    FiniteGuard.hpp
)

add_subdirectory(test)
