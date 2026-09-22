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

    # The windows preset's Clang enables a much broader diagnostic set than -Wall -Wextra ask
    # for (confirmed: plain -Wall -Wextra alone produces none of these on the same source with
    # this Clang). Every GTEST_TEST_F expansion then reports pre-C++20 "library compatibility"
    # style warnings against gnu++20 code that never targets C++98/pre-C++17, at a volume (tens
    # of thousands of lines across the test suite) that risks tripping CI's log-size limits
    # independent of any real defect. None of these bear on correctness here.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++20-compat
                             -Wno-pre-c++14-compat -Wno-pre-c++17-compat -Wno-pre-c++20-compat
                             -Wno-global-constructors -Wno-exit-time-destructors -Wno-weak-vtables
                             -Wno-padded)
    endif()
endfunction()
