add_library(e_foc.tool.virtual_motor.port ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_include_directories(e_foc.tool.virtual_motor.port PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
)

target_sources(e_foc.tool.virtual_motor.port PRIVATE
    PlantPortCodec.cpp
    PlantPortCodec.hpp
    PlantPortMessages.cpp
    PlantPortMessages.hpp
    PlantPortPayload.cpp
    PlantPortPayload.hpp
)

add_subdirectory(test)
