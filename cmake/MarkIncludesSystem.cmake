# Third-party headers must not fail our build under -Wall -Wextra with
# CMAKE_COMPILE_WARNING_AS_ERROR, so their interface includes are re-published as SYSTEM.
#
# Call site matters: this marks every target that exists when it runs. Call it after the
# submodule and FetchContent additions -- otherwise dependencies such as cucumber_cpp are
# missed -- and before add_subdirectory(core), or it marks this project's own targets SYSTEM
# and silences the warnings the flags exist to catch.
function(e_foc_mark_includes_system directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "UTILITY")
            get_target_property(includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
            if(includes)
                set_target_properties(${target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${includes}")
            endif()
        endif()
    endforeach()

    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        e_foc_mark_includes_system("${child}")
    endforeach()
endfunction()
