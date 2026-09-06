# These cannot live in CMakePresets.json: a preset condition is evaluated before configure,
# so it cannot branch on CMAKE_CXX_COMPILER_ID, and the guards below are load-bearing.
# -Wmaybe-uninitialized does not exist in Clang, which the windows preset uses, so passing
# -Wno-maybe-uninitialized there raises -Wunknown-warning-option and CMAKE_COMPILE_WARNING_AS_ERROR
# turns that into a failed build.
function(e_foc_enable_project_warnings)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra)
    endif()

    # At -O3 GCC inlines infra::Function::operator() into our translation units and reports
    # -Wmaybe-uninitialized against the caller, naming an index into a single object. Definite
    # -Wuninitialized stays on; see AGENTS.md.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wno-maybe-uninitialized)
    endif()
endfunction()
