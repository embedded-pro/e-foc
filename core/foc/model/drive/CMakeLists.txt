add_library(e_foc.foc.model.drive STATIC)

target_include_directories(e_foc.foc.model.drive PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>"
)

target_sources(e_foc.foc.model.drive PRIVATE
    PlantDrive.cpp
    PlantDrive.hpp
    PlantEvents.cpp
    PlantEvents.hpp
    SilPlantConfig.hpp
    SilPlantModel.hpp
)

target_link_libraries(e_foc.foc.model.drive PUBLIC
    e_foc.foc.model
    e_foc.platform_abstraction
)

add_subdirectory(test)
