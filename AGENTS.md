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
- `core/state_machine/` — Motor lifecycle FSM (`Idle` → `Calibrating` → `Ready` ⇄ `Enabled`, `Fault`). `TransitionPolicy::{Cli,Auto}`. `std::variant` states.
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
- **Decoupling**: ω·Ld·Iq feedforward on Vd; −ω·Lq·Id on Vq where appropriate
- Reuse `TransformsClarkePark` and `SpaceVectorModulation` from `core/foc/transforms/` — do not reimplement
- Unit types: `Ampere`, `Radians`, `Volts`, `RevPerMinute`, `PhasePwmDutyCycles`, `PhaseCurrents`

## Style

- Allman braces, 4-space indent, `.clang-format` authoritative
- `{}` init everywhere. PascalCase types/methods, camelCase members, lowercase namespaces
- Fixed-width ints (`uint8_t`, `int32_t`). Functions ≤ ~30 lines
- `const`/`constexpr`-correct. Non-trivial logic in `.cpp` — small `inline`/`constexpr` helpers in headers ok
- **No comments** except non-obvious *why*, unit/frame types don't carry, concurrency contract. One short line max. No `TODO`/`FIXME`/`HACK`, no commented-out code

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
- All visuals: Mermaid code blocks or ASCII art — no external image references
- Templates: `documentation/templates/`

## Build

```bash
cmake --preset host && cmake --build --preset host-Debug   # configure + build host
ctest --preset host                                         # run tests
cmake --preset EK-TM4C1294XL && cmake --build --preset EK-TM4C1294XL-Debug  # embedded
```

Presets: `host`, `coverage`, `EK-TM4C1294XL`, `EK-TM4C123GXL`, `STM32F407G-DISC1`, `NUCLEO-H563ZI`, `qemu-foc-sensored`.

**CI — two-tier**: `ci.yml` builds all targets and uploads `e_foc`, `e_foc.qemu_sil_tests`, and `e_foc.sync_foc_sensored.qemu.elf` as artifacts. `software-in-the-loop-tests.yml` triggers via `workflow_run` on CI completion, downloads those artifacts, and runs behavioral tests — never recompiles. Do not add cmake build steps to the SIL workflow.

**Embedded cmake**: call `halst_target_bringup(<target>)` for ST or `hal_ti_target_bringup(<target>)` for TI in `targets/*/main/CMakeLists.txt`. The `*_default_init` variants were removed.

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

## Assistant behavior — be terse

- Minimal prose. No preamble/postamble, no restating the plan, no summaries unless asked
- Report results as file paths + pass/fail. Don't narrate routine tool calls
- Don't re-read files already read; batch reads; prefer targeted edits
