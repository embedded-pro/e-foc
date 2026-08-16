add_library(e_foc.foc.transforms ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.foc.transforms PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.foc.transforms PUBLIC
    e_foc.foc.interfaces
    numerical.math
)

target_sources(e_foc.foc.transforms PRIVATE
    SpaceVectorModulation.cpp
    SpaceVectorModulation.hpp
    TransformsClarkePark.cpp
    TransformsClarkePark.hpp
)

add_subdirectory(test)
