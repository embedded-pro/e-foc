---
description: "e-foc CMake specifics: host test target template and the library targets to link."
applyTo: "**/CMakeLists.txt"
---

# e-foc CMake Specifics

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

## Library targets

Link these. Do not copy their logic.

- `infra.util` — bounded containers (`infra::BoundedVector`, `BoundedString`, `BoundedDeque`, `BoundedList`),
  `WithSharedAccess`
- `numerical.controllers.interfaces`, `numerical.controllers.implementations` — PID and other controllers
- `numerical.filters.passive` — filters (`filters::passive::Fir`, …)
- `numerical.math` — math helpers, `CompilerOptimizations.hpp`
