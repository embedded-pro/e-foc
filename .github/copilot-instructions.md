# e-foc — Copilot / AI agent instructions

This file is a concise, task-oriented guide for AI coding agents to be immediately productive in this repository.

1) Big-picture architecture (short)
- Purpose: Field-Oriented Control (FOC) for BLDC/PMSM with strict realtime and memory constraints.
- Major components:
  - `core/` — FOC implementations, platform abstraction interfaces, and services (libraries only).
  - `core/services/` — Application-level services (coordination, scheduling, helpers).
  - `core/platform_abstraction/` — Abstract `PlatformFactory` interface and shared adapters.
  - `targets/` — Application entry points (`hardware_test`, `sync_foc_sensored`) and platform implementations under `targets/platform_implementations/` (Host, ti, st).
  - `numerical-toolbox/` — Generic numerical algorithms (PID, filters, fixed-point helpers). Located at `infra/numerical-toolbox/`.
  - `embedded-infra-lib/` — Infrastructure: bounded containers, build helpers, toolchain cmake pieces. Located at `infra/embedded-infra-lib/`.
  - `tools/simulator/` — Host simulation models for validation.
  - `tools/can_commander/` — CAN bus command interface tool.

2) Critical developer workflows (exact commands)
- Clone (with submodules):
  - `git clone --recursive <repo>`
- Configure & build host (recommended first step):
  - `cmake --preset host`
  - `cmake --build --preset host-Debug`
- Run unit tests (GoogleTest):
  - `ctest --preset host-Debug`
- Build embedded target (example board):
  - `cmake --preset EK-TM4C1294XL`
  - `cmake --build --preset EK-TM4C1294XL-Debug`
- Coverage/analysis presets are defined in `CMakePresets.json` — use `coverage` preset for coverage builds.

3) Project-specific constraints and conventions (must follow these)
- NO HEAP: avoid `new/delete`, `malloc/free`, `std::make_unique`, etc.
- NO dynamic STL containers in embedded code: use `infra::BoundedVector`, `infra::BoundedString`, etc. (see `embedded-infra-lib`).
- Avoid `virtual ~Dtor() = 0` (pure virtual destructors) — they add significant flash/RAM overhead. The default is **no pure virtual destructor**.
- Prefer fixed-size integer types (`uint8_t`, `int32_t`, ...).
- Avoid recursion and virtual calls in ISR/hot paths.
- Favor `constexpr`, `inline`, and `const` correctness for performance.

4) Patterns & code locations (concrete examples)
- Add a new FOC algorithm:
  - Implement code in `core/foc/implementations/` and keep public interfaces in `core/foc/interfaces/`.
  - Motor-specific application code lives under `targets/sync_foc_sensored/` and `targets/hardware_test/`.
- Platform abstraction & factory: see `core/platform_abstraction/PlatformFactory.hpp` for how peripherals and adapters are created and injected.
- Numerical algorithms: follow patterns in `infra/numerical-toolbox/` — implement float first, then Q15/Q31 variants, and add typed GoogleTest suites.

5) Testing & CI expectations
- Unit tests run on host using GoogleTest. Use typed tests for multiple numeric types (float, Q15, Q31).
- Prefer small, deterministic tests that do not require hardware.
- If adding platform-specific tests, provide host stubs/mocks in `targets/platform_implementations/Host/`.
- Always use `testing::StrictMock<>` for all mock instances — `NiceMock` and `NaggyMock` are **forbidden**.

6) Build system tips
- Presets are the primary interface: see `CMakePresets.json` to pick host vs embedded and board presets.
- Toolchains for embedded boards live under `infra/embedded-infra-lib/cmake/toolchain-*.cmake`.
- `compile_commands.json` is generated in build dirs; use it for language server/analysis.

7) What to preserve from existing docs
- There is an existing, detailed guidance file for the numerical toolbox at `infra/numerical-toolbox/.github/copilot-instructions.md` — preserve algorithm-level constraints from there when editing numerical code.

8) When making changes, be explicit
- Update corresponding `doc/` entries for algorithms and any example in `examples/`.
- For algorithmic code, include numerical properties (stability, range, complexity) in `doc/`.

9) Quick pointers for reviewers / code suggestions
- If suggesting new APIs, prefer interface-driven DI and small, testable functions.
- For performance changes, provide before/after size/runtime metrics and ensure host tests cover correctness.

10) Performance optimization
- For performance-critical code (FOC, PID, ISRs), see `documentation/performance-optimization/README.md`.
- Key techniques: avoid virtual dispatch in hot paths, use `#pragma GCC optimize("O3", "fast-math")` for critical files, prefer static inline functions over virtual methods.
- Debug builds use `-Og` to maintain debuggability while enabling basic optimizations.
- Use `arm-none-eabi-objdump -d -C` to analyze generated assembly and verify optimizations.
- Target cycle budgets: FOC loop should complete in <400 cycles at 120 MHz for 20 kHz control rate.

If any section appears incomplete or you want deeper coverage (build-on-target, hardware flashing steps, or CI specifics), tell me which area to expand.
