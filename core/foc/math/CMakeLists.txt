add_library(e_foc.foc.math INTERFACE ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.math INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.math INTERFACE
    numerical.math
)

target_sources(e_foc.foc.math INTERFACE
    AngleWrap.hpp
    FastTrigonometry.hpp
)

add_subdirectory(test)
