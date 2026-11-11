add_library(e_foc.foc.model.drive.memory STATIC)

target_include_directories(e_foc.foc.model.drive.memory PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
)

target_sources(e_foc.foc.model.drive.memory PRIVATE
    NvmImageBuilder.cpp
    NvmImageBuilder.hpp
    PlantCalibration.cpp
    PlantCalibration.hpp
)

target_link_libraries(e_foc.foc.model.drive.memory PUBLIC
    e_foc.foc.model.drive
    e_foc.services.non_volatile_memory
    infra.util
)

add_subdirectory(test)
