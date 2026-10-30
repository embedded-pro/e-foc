---
description: "e-foc CMake rules: halst_target_bringup/hal_ti_target_bringup for embedded targets, add_subdirectory(test) for test targets, bounded-container and numerical-toolbox library targets, two-tier CI artifact model."
applyTo: "**/CMakeLists.txt"
---

# e-foc CMake Rules

Follow AGENTS.md — see §Build (build commands and presets, CI artifact model, new files, embedded bringup, warnings and optimisation placement) and §Testing (`add_subdirectory(test)`). This file adds only the CMake specifics.

## Test targets

A library's `test/CMakeLists.txt` declares its own host-only test executable and registers it with EMIL:

```cmake
add_executable(e_foc.foc.transforms_test)
emil_build_for(e_foc.foc.transforms_test HOST All BOOL E_FOC_BUILD_TESTS)
emil_add_test(e_foc.foc.transforms_test)

target_link_libraries(e_foc.foc.transforms_test PUBLIC
    gmock_main
    e_foc.foc.transforms
)
```

## Key library targets

Link against these rather than duplicating their logic:
- `infra.util` — bounded containers (`infra::BoundedVector`, `BoundedString`, `BoundedDeque`, `BoundedList`), `WithSharedAccess`
- `numerical.controllers.interfaces`, `numerical.controllers.implementations` — PID and other controllers
- `numerical.filters.passive` — filters (`filters::passive::Fir`, …)
- `numerical.math` — math helpers, `CompilerOptimizations.hpp`
