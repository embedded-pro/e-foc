# Embedded builds give all of e-foc one set of optimisation options, never one per function: GCC
# does not inline a callee whose options differ from its caller's, so a `#pragma GCC optimize` or
# `optimize` attribute on a hot function turns every small helper it calls into an out-of-line call.
# -fno-finite-math-only keeps std::isnan and std::isfinite meaningful; the rest of -ffast-math stays.
function(e_foc_enable_embedded_optimizations)
    if(E_FOC_EMBEDDED_BUILD AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-ffast-math;-fno-finite-math-only>")
    endif()
endfunction()
