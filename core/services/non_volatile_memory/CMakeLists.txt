add_library(e_foc.services.non_volatile_memory ${EMIL_EXCLUDE_FROM_ALL})

target_include_directories(e_foc.services.non_volatile_memory PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(e_foc.services.non_volatile_memory PUBLIC
    infra.util
    hal.interfaces
    services.util
)

target_sources(e_foc.services.non_volatile_memory PRIVATE
    CalibrationData.hpp
    ConfigData.hpp
    FaultHistoryData.hpp
    FaultHistoryData.cpp
    LearnedModelData.hpp
    LearnedModelData.cpp
    NonVolatileMemory.hpp
    NvmBytes.hpp
    NvmRecord.hpp
    NonVolatileMemoryImpl.hpp
    NonVolatileMemoryImpl.cpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
