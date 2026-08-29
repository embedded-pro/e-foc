---
description: "e-foc CMake rules: halst_target_bringup/hal_ti_target_bringup for embedded targets, add_subdirectory(test) for test targets, bounded-container and numerical-toolbox library targets, two-tier CI artifact model."
applyTo: "**/CMakeLists.txt"
---

# e-foc CMake Rules

## Embedded target bringup

When wiring a new target in `targets/*/main/CMakeLists.txt`:
- ST targets: `halst_target_bringup(<target>)`
- TI targets: `hal_ti_target_bringup(<target>)`
- `*_default_init` variants were removed — do not use them.

## Test targets

Add tests via `add_subdirectory(test)` inside the library's `CMakeLists.txt`. The test directory must
contain its own `CMakeLists.txt` that registers targets with `gtest_discover_tests`.

## Key library targets

- `infra::BoundedVector` and other bounded containers — via `infra/embedded-infra-lib/`
- `numerical::PidController`, `numerical::FirFilter`, etc. — via `infra/numerical-toolbox/`
- Link against these rather than duplicating their logic.

## CI artifacts (do not add build steps to SIL workflow)

`ci.yml` builds and uploads: `e_foc`, `e_foc.qemu_sil_tests`, `e_foc.sync_foc_sensored.qemu.elf`.
`software-in-the-loop-tests.yml` downloads those artifacts and runs tests — never recompiles.
Do not add `cmake` or `cmake --build` steps to `software-in-the-loop-tests.yml`.

## Build commands (reference)

```bash
cmake --preset host && cmake --build --preset host-Debug
ctest --preset host
cmake --preset EK-TM4C1294XL && cmake --build --preset EK-TM4C1294XL-Debug
cmake --preset qemu-foc-sensored && cmake --build --preset qemu-foc-sensored-Debug
```
