function(e_foc_generate_build_info)
    set(revision "unknown")

    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --short=8 HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE git_revision
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE git_result
            ERROR_QUIET
        )

        if(git_result EQUAL 0 AND NOT git_revision STREQUAL "")
            set(revision "${git_revision}")
        endif()
    endif()

    set(E_FOC_SOURCE_REVISION "${revision}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildInfo.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/e_foc/BuildInfo.hpp"
        @ONLY
    )

    add_library(e_foc.build_info INTERFACE)
    target_include_directories(e_foc.build_info INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endfunction()
