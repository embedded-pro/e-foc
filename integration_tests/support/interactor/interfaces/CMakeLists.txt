add_library(e_foc.integration_tests.interactor.interfaces OBJECT
    TargetInteractor.hpp
    TargetInteractor.cpp
)

target_include_directories(e_foc.integration_tests.interactor.interfaces PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../..>"
)

target_link_libraries(e_foc.integration_tests.interactor.interfaces PUBLIC
    hal.interfaces
)
