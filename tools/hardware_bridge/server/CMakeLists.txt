if(E_FOC_BUILD_TESTS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_test(
        NAME e_foc.tool.hardware_bridge.server_python_test
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_LIST_DIR}/test -v
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    )
endif()
