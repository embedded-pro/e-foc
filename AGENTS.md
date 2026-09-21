# e-foc — Agent Rules (canonical)

Single source of truth for **Claude, Copilot, and sub-agents**. `CLAUDE.md` and `.github/copilot-instructions.md` point here. Sub-agent definitions: `.claude/agents/` (Claude) and `.github/agents/` (Copilot).

FOC implementation for BLDC/PMSM motors. Strict real-time and memory constraints targeting embedded MCUs.

## Architecture

- `core/foc/interfaces/` — FOC vocabulary/contracts (`Units.hpp`, `Signals.hpp`, `Foc.hpp`). No algorithms.
- `core/foc/transforms/` — Clarke/Park transforms, SVM
- `core/foc/math/` — `FastTrigonometry.hpp`, `AngleWrap.hpp` (header-only, not FOC-specific)
- `core/foc/cascade/` — Cascade orchestration + gain design; no hardware dependency
- `core/foc/instantiations/` — Concrete wiring (`Runner`, `FocController`)
- `core/platform_abstraction/` — `PlatformFactory` + hardware ports (`drivers::ThreePhaseInverter`, `drivers::Encoder`, `drivers::HallSensor`) in `interfaces/Drivers.hpp`
- `core/state_machine/` — Motor lifecycle FSM (`Idle` → `Calibrating` → `Ready` ⇄ `Enabled`, `Fault`) on EmIL `services::TableStateMachine`: `std::variant` states, `std::variant` events (`FocStateMachineEvents.hpp`), transition table in `FocLifecycleTable`. `TransitionPolicy::{Cli,Auto}` only selects terminal commands.
- `core/services/` — Alignment, CLI, system ID, NVM
- `targets/` — App entry points (`hardware_test`, `sync_foc_sensored`) + platform implementations (`host`, `ti`, `st`)
- `infra/numerical-toolbox/` — PID, filters, fixed-point math (see its own `AGENTS.md`)
- `infra/embedded-infra-lib/` — Bounded containers, build helpers, toolchain cmake
- `tools/simulator/` — Host simulation; `tools/can_commander/` — CAN interface

## Memory — no heap (embedded/runtime code)

**Scope**: `core/foc/`, `core/platform_abstraction/`, `core/state_machine/`, `targets/`, ISR-reachable paths. Host tools, simulators, and tests may use heap.

Forbidden: `new`/`delete`/`malloc`/`free`, `make_unique`/`make_shared`, `std::vector`/`string`/`deque`/`list`/`map`/`set`. No recursion. No `virtual ~D() = 0`.

Use: `infra::BoundedVector<T>::WithMaxSize<N>`, `infra::BoundedString::WithStorage<N>`, `infra::BoundedDeque<T>::WithMaxSize<N>`, `std::array<T,N>`, `std::optional<T>`.

## Real-time — FOC loop

`Calculate()` runs at 20 kHz in interrupt. Budget: **≤4500 cycles at 120 MHz** (inner loop); **≤20000** (1 kHz outer loop).

Forbidden in hot path: virtual dispatch, heap, blocking calls, raw `sin`/`cos` — use `FastTrigonometry` from `core/foc/math/FastTrigonometry.hpp`.

Also forbidden: per-sample validation. No plausibility check on the phase currents, no finiteness check on
currents or duty cycles — see REQ-PERF-003. Sensor and power-stage failures are the hardware protection path's
to catch. `foc::IsFiniteValue` exists for configuration-time and outer-loop checks only
(`CurrentPlantModel::IsUsable`, the mechanical estimator's plausibility band); it is not for `Calculate()`.

Required in every hot-path file:

```cpp
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif
#include "numerical/math/CompilerOptimizations.hpp"
```

`OPTIMIZE_FOR_SPEED` on `Calculate()`, `Compute()`, and other hot-path methods.

## FOC theory — correctness

- **Clarke**: `Iα = (2/3)·(Ia − (Ib+Ic)/2)`, `Iβ = (Ib − Ic)/√3` (amplitude-invariant, 3 phases)
- **Park**: `Id = Iα·cos(θ) + Iβ·sin(θ)`, `Iq = −Iα·sin(θ) + Iβ·cos(θ)`
- **Electrical angle**: `θe = θm · pole_pairs`
- **Anti-windup**: all PID integrators must clamp or use back-calculation
- **Decoupling**: −ω·Lq·Iq feedforward on Vd; +ω·(Ld·Id + ψf) on Vq. The Vd term is negative and the Vq term positive — these cancel the coupling the plant adds, so getting the signs backwards doubles it
- Reuse `TransformsClarkePark` and `SpaceVectorModulation` from `core/foc/transforms/` — do not reimplement
- Unit types: `Ampere`, `Radians`, `Volts`, `RevPerMinute`, `PhasePwmDutyCycles`, `PhaseCurrents`

## Style

- Allman braces, 4-space indent, `.clang-format` authoritative
- `{}` init everywhere. PascalCase types/methods, camelCase members, lowercase namespaces
- Fixed-width ints (`uint8_t`, `int32_t`). Functions ≤ ~30 lines
- `const`/`constexpr`-correct. Non-trivial logic in `.cpp` — small `inline`/`constexpr` helpers in headers ok
- **No comments** except non-obvious *why*, unit/frame types don't carry, concurrency contract. One short line max. No `TODO`/`FIXME`/`HACK`, no commented-out code

## Async callback lifetime safety

Any service that schedules event-dispatcher closures and whose lifetime is managed by an FSM `std::variant` **must** follow this pattern (full design: `documentation/design/async-callback-lifetime.md`):

1. **Inherit `infra::EnableSharedFromThis<T>`** on the service class.
2. **Schedule via WeakPtr** — use `infra::EventDispatcherWithWeakPtr::Instance().Schedule([](const infra::SharedPtr<T>& self){ … }, WeakFromThis())`. Raw `[this]` captures on the dispatcher are forbidden.
3. **Wrap at construction** — every construction site uses `infra::WithSharedAccess<T>` (from `infra/util/WithSharedAccess.hpp`), not a bare member or local variable.
4. **Expose `IsRunning() const`** on the service interface; return `true` while estimation state is active.
5. **Extend `HasPendingAsyncWork()`** — the `FocStateMachine` subclass that owns the service must OR `service.IsRunning()` into its `HasPendingAsyncWork()` override; omitting this causes mode-switch to proceed while hardware is still driven.
6. **Test fixtures** — any fixture that holds a `WithSharedAccess`-wrapped member must call `ExecuteAllActions()` in `TearDown()`, and all `StrictMock` service mocks must have `EXPECT_CALL(…, IsRunning()).WillRepeatedly(Return(false))` in the teardown-setup block.

*Static objects* (application-level composites that are never destroyed while the event loop runs) are exempt from rules 1–3.

## Interfaces & errors

- Interfaces = pure virtual; `virtual ~I() = default` — **never** `= 0` destructors
- No exceptions. `std::optional<T>` or status enums. `assert()`/`really_assert()` for preconditions
- SOLID + DIP: constructor injection via `PlatformFactory`; depend on abstractions. No global state.

## Testing

- GoogleTest. `TEST_F` for fixture tests; `TYPED_TEST` for typed; plain `TEST()` for simple stateless matching existing patterns
- **`StrictMock` only** (`NiceMock`/`NaggyMock` forbidden). Fixture in anonymous namespace; macros outside
- `EXPECT_NEAR` with explicit tolerance for float assertions. No heap in tests.
- Test files: `test/Test{ComponentName}.cpp` inside the library under test
- Host stubs: `targets/platform_implementations/host/`

## Documentation — always updated

Documentation-first: update `documentation/` **before or alongside** behavioral code changes.

- `documentation/theory/` — FOC algorithm or motor model changes
- `documentation/performance-optimization/README.md` — timing-critical changes
- All visuals: Mermaid (flowcharts/state/sequence), TikZ `{=latex}` raw blocks (technical charts, signal flows, coordinate frames — source in `documentation/tikz/`), or ASCII art — no external image references
- Templates: `documentation/templates/`

## Build

```bash
cmake --preset host && cmake --build --preset host-Debug   # configure + build host
ctest --preset host                                         # run tests
cmake --preset EK-TM4C1294XL && cmake --build --preset EK-TM4C1294XL-Debug  # embedded
```

Presets: `host`, `coverage`, `EK-TM4C1294XL`, `EK-TM4C123GXL`, `STM32F407G-DISC1`, `NUCLEO-H563ZI`, `qemu-foc-sensored`.

`qemu-foc-sensored` builds `e_foc.sync_foc_sensored.main` for emulated Cortex-M4 via semihosting. The resulting ELF is uploaded as a CI artifact for the Software-in-the-Loop Tests job.

**CI — single workflow**: `ci.yml` builds all targets and uploads `e_foc`, `e_foc.integration_tests`, and `e_foc.sync_foc_sensored.qemu.elf` as artifacts. The `software_in_the_loop_tests` job runs in the same workflow with `needs: [host_build_test_ubuntu, qemu_foc_sensored_build]`, downloads those artifacts, and runs behavioral tests — never recompiles. Do not add cmake build steps to that job.

**Before pushing**: always verify locally before pushing — CI minutes and tokens are finite.

```bash
# 1. Full host build must succeed
cmake --preset host && cmake --build build/host --config Debug -j$(nproc)

# 2. Unit tests must pass (one pre-existing failure in electrical_system_ident is known)
ctest --test-dir build/host -C Debug -E "e_foc.integration_tests" --output-on-failure

# 3. SIL binary must link (QEMU firmware required for runtime correctness)
cmake --preset qemu-foc-sensored && cmake --build build/qemu-foc-sensored --target e_foc.sync_foc_sensored.main -j$(nproc)
QEMU_SIL_ELF=build/qemu-foc-sensored/targets/sync_foc_sensored/main/RelWithDebInfo/e_foc.sync_foc_sensored.main.elf \
  build/host/integration_tests/main/Debug/e_foc.integration_tests \
  --mode sil integration_tests/features/ -t "@sil"
```

**Embedded cmake**: call `halst_target_bringup(<target>)` for ST or `hal_ti_target_bringup(<target>)` for TI in `targets/*/main/CMakeLists.txt`. The `*_default_init` variants were removed.

Authoritative cycle budget gate: `cortex-cycle-budget` static analysis (≤ 4500 inner / ≤ 20000 outer). It measures only the symbols its config whitelists, so adding a function to a hot path means adding it to `targets/sync_foc_sensored/main/cycle-analysis.json` (inner) or `cycle-analysis-outer.json` (outer) — an unlisted symbol is silently unmeasured, not flagged.

**Warnings**: `cmake/CompilerWarnings.cmake` provides `e_foc_enable_project_warnings()`, which enables
`-Wall -Wextra`; with `CMAKE_COMPILE_WARNING_AS_ERROR` on, any warning in `core/`, `targets/`,
`integration_tests/` or `tools/` fails the build. Third-party include directories are marked SYSTEM by
`e_foc_mark_includes_system` from `cmake/MarkIncludesSystem.cmake`, so warnings in headers we do not own
cannot. That call marks every target that exists when it runs, which is why the **call** sits after the
submodule and FetchContent additions and **before** `add_subdirectory(core)`: move it later and it would
mark this project's own targets SYSTEM, silencing the warnings the flags exist to catch; move it earlier and
FetchContent dependencies such as `cucumber_cpp` would be missed. Never silence a warning with a pragma —
fix it, or drop the parameter name if it is genuinely unused.

Neither belongs in `CMakePresets.json`. A preset condition is evaluated before configure, so it cannot
branch on `CMAKE_CXX_COMPILER_ID`, and both guards are load-bearing — see below.

`-Wmaybe-uninitialized` is off, for GNU only. SYSTEM marking does not help here: at `-O3` GCC inlines
`infra::Function::operator()` into our translation units and reports the diagnostic against the caller,
naming addresses like `((const infra::Function<...>*)this)[1319]` — an index into a single object, so the
path it claims does not exist. Definite `-Wuninitialized` stays enabled and is the one that matters; it is
what caught `RecursiveLeastSquares` copying an indeterminate `metrics` member. The GNU guard matters because
Clang has no `-Wmaybe-uninitialized`: the `windows` preset builds with
`toolchain-clang-x86_64-pc-windows-msvc.cmake`, where the flag would raise `-Wunknown-warning-option` and
warnings-as-errors would fail the build.

**Features**: all scenarios live in `integration_tests/features/`, tagged by the runner that implements them: `@sil` for the QEMU software-in-the-loop runner, `@hil` for hardware. `@sil-protection` holds the board-protection scenarios, which are kept out of the default run because they reproduce a firmware lockup, and `@sil-known-defect` holds the performance scenarios whose law does not meet its envelope today — both documented in `documentation/design/software-in-the-loop.md`. The `defaults` test preset excludes the `hardware|integration` labels, so `ctest --preset host` does **not** run the integration suite; invoke it directly, as the SIL command block above does.

**Software-in-the-loop**: the motor plant is an input to the target, not a constant. The harness writes a plant description and an NVM image into a per-scenario working directory the emulator runs from, so the firmware reads both by relative name. Design: `documentation/design/software-in-the-loop.md`. `SIL_VERBOSE=1` traces both directions and keeps the scenario directory; `SIL_GDB=1` starts the emulator halted with a GDB stub on port 1234.

**SIL performance scenarios**: the plant reports its trajectory as `PLANT` lines on the trace channel (control-tick time base, never printed from the ISR), a shaft torque step is scheduled in the plant description, and `integration_tests/support/response/` measures the step and disturbance metrics with the numerical toolbox (`numerical/math/StepResponseMetrics.hpp`). Every metric assertion prints a `[METRIC]` line; limits are pinned from those lines with a margin, never derived from theory alone.

**Reference motors**: `motor_parameters/` holds the two motors the plant is exercised with (Teknic M-2310P-LN-04K, Anaheim BLY172S-24V-4000), with the sources in `documentation/theory/foc-plant-models.md` "Reference motors". The SIL presets `teknic` and `anaheim` select them; `nominal` is the Teknic. Identification scenarios (`parameter_identification.feature`) compare what the firmware identifies (CAN electrical response, `[SM] Calibration record:` and `[EST]` trace lines, all mechanical values in micro-units) against the plant the scenario configured. In SIL a terminal command is a line without the `CAN_RX` prefix on the same socket; the answer arrives as trace lines, never as a prompt to wait for.

## Agent routing

- **orchestrator** — first stop for any non-trivial task; triages to specialists
- **planner** — new FOC modes, architectural changes, multi-file changes, tasks needing upfront design
- **executor** — bug fixes, small changes, tasks with a clear existing plan; follows TDD
- **reviewer** — reviewing code or recent changes against project standards
- **analyst** — investigation, audit, root-cause analysis, design exploration; read-only, no code changes

## Constraints checklist

Before finalizing any plan or implementation, verify:

**Memory (embedded/runtime scope)**
- [ ] No `new`/`delete`/`malloc`/`free`/`make_unique`/`make_shared`
- [ ] No `std::vector`/`string`/`deque`/`list`/`map`/`set` — use bounded alternatives
- [ ] No recursion; no `virtual ~D() = 0`

**Real-time**
- [ ] No virtual dispatch in `Calculate()` hot path
- [ ] No blocking calls or heap reachable from `Calculate()`
- [ ] `FastTrigonometry` used — not raw `sin`/`cos`
- [ ] `#pragma GCC optimize("O3","fast-math")` present (guarded); `OPTIMIZE_FOR_SPEED` on hot-path methods

**FOC theory**
- [ ] Clarke: `Iα=(2/3)·(Ia−(Ib+Ic)/2)`, `Iβ=(Ib−Ic)/√3`; Park: `Id=Iα·cos(θ)+Iβ·sin(θ)`, `Iq=−Iα·sin(θ)+Iβ·cos(θ)`
- [ ] `θe=θm·pole_pairs`; anti-windup on all PIDs; decoupling feedforward present
- [ ] No reimplementation of `TransformsClarkePark`/`SpaceVectorModulation`
- [ ] Unit-typed aliases used — not raw `float`

**Design**
- [ ] Hardware injected via constructor; no global state
- [ ] `documentation/` updated before or alongside behavioral changes
- [ ] New files added to `CMakeLists.txt`; tests added via `add_subdirectory(test)`

**Async callback lifetime safety**
- [ ] No `EventDispatcherWithWeakPtr::Instance().Schedule([this]…)` raw captures on non-static objects
- [ ] Services using WeakPtr dispatch inherit `EnableSharedFromThis<T>` and are wrapped in `WithSharedAccess<T>` at every construction site
- [ ] New identification/async services expose `IsRunning()` and it is included in `HasPendingAsyncWork()`
- [ ] Test fixtures with `WithSharedAccess` members call `ExecuteAllActions()` in `TearDown()`; `StrictMock` service mocks expect `IsRunning().WillRepeatedly(Return(false))`

## Assistant behavior — be terse

- Minimal prose. No preamble/postamble, no restating the plan, no summaries unless asked
- Report results as file paths + pass/fail. Don't narrate routine tool calls
- Don't re-read files already read; batch reads; prefer targeted edits
