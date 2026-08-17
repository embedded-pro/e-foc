# e-foc — Claude Code Instructions

This file is a concise, task-oriented guide for Claude and AI agents to be immediately productive in this repository.

## 1. Big-Picture Architecture

**Purpose**: Field-Oriented Control (FOC) for BLDC/PMSM motors with strict real-time and memory constraints targeting embedded microcontrollers.

**Major components**:
- `core/` — FOC implementations, platform abstraction interfaces, and services (libraries only).
- `core/foc/interfaces/` — FOC vocabulary and contracts: `Units.hpp`, `Signals.hpp` (`PhaseCurrents`, `PhasePwmDutyCycles`, `ThreePhase`, `TwoPhase`, `RotatingFrame`), `Foc.hpp` (`FocBase`, `FocTorque`, `FocSpeed`, `FocPosition`), `Execution.hpp` (`LowPriorityInterrupt`, `Controllable`), `OnlineEstimators.hpp`. No algorithms.
- `core/foc/math/` — Header-only generic numerics: `FastTrigonometry.hpp` (sine LUT), `AngleWrap.hpp`. Not FOC-specific; candidate for upstreaming to `numerical-toolbox`.
- `core/foc/transforms/` — The FOC math proper: Clarke/Park transforms and Space Vector Modulation.
- `core/foc/cascade/` — Cascade orchestration (`TorqueCascade`, `SpeedCascade`, `PositionCascade`, `CascadeWithSpeedLoop`). Dispatches to the current and speed loop selectors; depends on no hardware.
- `core/foc/instantiations/` — Execution wiring: `Runner`, `LowPriorityInterruptImpl`, `FocController`. The only foc layer that touches hardware ports.
- `core/foc/instantiations/` — Concrete wiring of FOC components for specific targets.
- `core/services/` — Application-level services (alignment, CLI, system identification, NVM).
- `core/foc/selection/`, `core/foc/current_loop/`, `core/foc/speed_loop/` — Runtime-selectable control algorithms in the `foc` namespace, dispatched through a `std::variant` selector.
- `core/platform_abstraction/` — Abstract `PlatformFactory` interface and shared adapters.
- `core/state_machine/` — `FocStateMachineBase` (abstract interface in `FocStateMachine.hpp`) and `FocStateMachineCommon` (concrete in `FocStateMachineCommon.hpp`, `application` namespace): formal motor lifecycle state machine (`Idle` → `Calibrating` → `Ready` ⇄ `Enabled`, `Fault`). Uses `std::variant` for states and `state_machine::TransitionPolicy` enum (`::Cli` / `::Auto`) for transition mode.
- `targets/` — Application entry points (`hardware_test`, `sync_foc_sensored`) and platform implementations under `targets/platform_implementations/` (host, ti, st, motor_boards).
- `infra/numerical-toolbox/` — Generic numerical algorithms (PID, filters, fixed-point helpers).
- `infra/embedded-infra-lib/` — Infrastructure: bounded containers, build helpers, toolchain cmake pieces.
- `tools/simulator/` — Host simulation models for validation.
- `tools/can_commander/` — CAN bus command interface tool.

## 2. Critical Developer Workflows

```bash
# Clone (with submodules)
git clone --recursive <repo>

# Configure & build host (recommended first step)
cmake --preset host
cmake --build --preset host-Debug

# Run unit tests (GoogleTest) — use 'host' test preset
ctest --preset host

# Build embedded target
cmake --preset EK-TM4C1294XL
cmake --build --preset EK-TM4C1294XL-Debug

# Coverage build
cmake --preset coverage
cmake --build --preset coverage
```

All presets are defined in `CMakePresets.json`. Available configure presets: `host`, `coverage`, `EK-TM4C1294XL`, `EK-TM4C123GXL`, `STM32F407G-DISC1`, `NUCLEO-H563ZI`.

## 3. Project-Specific Constraints (must follow)

### Memory — No Heap in Embedded/Real-Time Code

The no-heap rule applies to `core/foc/`, `core/platform_abstraction/`, `core/state_machine/`, `targets/`, ISR-reachable paths. Host tools, simulators, and test code may use normal heap patterns.

- No `new`, `delete`, `malloc`, `free`, `std::make_unique`, `std::make_shared`
- No `std::vector` → use `infra::BoundedVector<T>::WithMaxSize<N>`
- No `std::string` → use `infra::BoundedString::WithStorage<N>`
- No `std::deque`/`std::list`/`std::map`/`std::set` → use bounded alternatives from `embedded-infra-lib`
- No recursion in embedded/runtime paths (stack must be predictable)
- No `virtual ~Dtor() = 0` (pure virtual destructors) — adds flash/RAM overhead. Default: **no pure virtual destructor**.

### Real-Time — FOC Loop

The `Calculate()` method runs at 20 kHz in interrupt context.

- No virtual dispatch in `Calculate()` hot path
- No blocking calls in ISR-reachable code
- Budget: the 20 kHz ISR path must stay within **4500 cycles at 120 MHz**, 75% of the 6000-cycle control period, as required by `documentation/architecture/system.md`. It currently measures 1074–1353 cycles (~23%), of which the FOC calculation itself is 739–860.
- Use `FastTrigonometry` (from `core/foc/math/FastTrigonometry.hpp`) or lookup tables — not raw `sin`/`cos` in hot paths

Every implementation file with hot-path code must include:

```cpp
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif
```

Apply `OPTIMIZE_FOR_SPEED` (from `numerical/math/CompilerOptimizations.hpp`) to `Calculate()`, `Compute()`, and other hot-path methods.

### Other Constraints

- Prefer fixed-size integer types (`uint8_t`, `int32_t`).
- Avoid large implementations in headers; keep non-trivial logic in `.cpp` files. Small `inline`/`constexpr` helpers in headers are allowed (and common in hot paths).
- Prefer `{}` initialization over `()` for all variables and member data.
- `const` on all non-mutating methods, `constexpr` for motor constants and lookup tables.

## 4. FOC Theory — Correctness

- **Clarke**: `Iα = (2/3)·(Ia - (Ib+Ic)/2)`, `Iβ = (Ib - Ic)/√3` (amplitude-invariant, all 3 phases used)
- **Park**: `Id = Iα·cos(θ) + Iβ·sin(θ)`, `Iq = -Iα·sin(θ) + Iβ·cos(θ)`
- **Electrical angle**: `θe = θm · pole_pairs`
- **Anti-windup**: All PID integrators must have clamping or back-calculation
- **Decoupling**: Add ω·Ld·Iq feedforward to Vd, subtract ω·Lq·Id from Vq where appropriate
- Reuse `Clarke`, `Park`, `ClarkePark` and `SpaceVectorModulation` from `core/foc/transforms/` — do not duplicate
- Use unit-typed aliases: `Ampere`, `Radians`, `Volts`, `RevPerMinute`, `PhasePwmDutyCycles`, `PhaseCurrents`

## 5. Naming Conventions

- **Classes/Methods**: `PascalCase` (`SpeedCascade`, `Calculate()`, `SetPoint()`)
- **Member variables**: `camelCase` (`polePairs`, `currentTunings`)
- **Namespaces**: lowercase (`foc`, `hardware`)

## 6. Brace Style — Allman, 4-Space Indent

```cpp
namespace foc
{
    class SpeedCascade
        : public FocSpeed
    {
    public:
        void SetPoint(RevPerMinute setPoint) override;
        PhasePwmDutyCycles Calculate(const PhaseCurrents& currentPhases, Radians& position) override;

    private:
        controllers::PidController<float> speedPid;
        std::size_t polePairs{ 0 };
    };
}
```

## 7. Patterns & Code Locations

- **New FOC algorithm**: implement in `core/foc/cascade/`, keep public interfaces in `core/foc/interfaces/`, put pure transforms in `core/foc/transforms/` and generic numerics in `core/foc/math/`
- **State machine**: see `core/state_machine/FocStateMachine.hpp` (base) and `core/state_machine/FocStateMachineCommon.hpp` (implementation)
- **Platform abstraction**: see `core/platform_abstraction/PlatformFactory.hpp`
- **Numerical algorithms**: follow patterns in `infra/numerical-toolbox/`
- **Host platform stubs**: `targets/platform_implementations/host/`

## 8. Testing

- Unit tests live in the `test/` folder of the library under test, e.g. `core/foc/transforms/test/Test{ComponentName}.cpp`
- Hardware stubs: `targets/platform_implementations/host/`
- Host simulation: `tools/simulator/`
- Use `TEST_F` for fixture tests; `TYPED_TEST` for multi-type tests; plain `TEST()` acceptable for simple stateless cases
- **`testing::StrictMock<>`** for ALL mock instances — `NiceMock` and `NaggyMock` are **FORBIDDEN**
- Fixture class inside `namespace {}`, test macros outside
- Follow Arrange-Act-Assert pattern
- Use `EXPECT_NEAR` with explicit tolerance for floating-point assertions

## 9. Comments

A comment must state something the code cannot, in one short line: a non-obvious *why* (hardware quirk, ordering requirement, deliberate deviation), a unit or frame the types do not carry (electrical vs mechanical, per-unit vs volts, per-sample vs continuous gain), or a concurrency contract (which context writes a field, which reads it).

Do not write comments that restate the next line, address a reviewer, describe your change, or claim behaviour the code does not implement. No section banners, no commented-out code, no `TODO`/`FIXME`/`HACK` in production code. In tests, a comment must never claim a verification the assertions do not perform.

A stale comment is worse than no comment — when you change code, delete or correct every comment that no longer matches it.

## 10. Documentation — Always Updated

- Documentation-first: for behavioral changes, update the corresponding architecture/design doc in `documentation/` **before or alongside** code changes. Code must follow documentation.
- `documentation/theory/` — update for any FOC algorithm or motor model change
- `documentation/performance-optimization/README.md` — update for any timing-critical change
- `documentation/templates/` — use as starting template for new documents
- All visuals in docs must use Mermaid code blocks or ASCII art — no external image references

## 11. Build System

- Presets are the primary interface: see `CMakePresets.json`
- Toolchains for embedded boards: `infra/embedded-infra-lib/cmake/toolchain-*.cmake`
- `compile_commands.json` generated in build dirs; use for language server/analysis

## 12. Performance Optimization

- See `documentation/performance-optimization/README.md` for key techniques
- Use `#pragma GCC optimize("O3", "fast-math")` and `OPTIMIZE_FOR_SPEED` for critical files
- Debug builds use `-Og` to maintain debuggability
- Use `arm-none-eabi-objdump -d -C` to analyze generated assembly

## 13. Numerical Toolbox

- Located at `infra/numerical-toolbox/`
- Has its own detailed guidance at `infra/numerical-toolbox/.github/copilot-instructions.md`
- When editing numerical code, preserve algorithm-level constraints from that file
- Implement float first, then Q15/Q31 variants; add typed GoogleTest suites

## 14. Design Principles

- **SOLID**: Single Responsibility, Open/Closed, Liskov Substitution, Interface Segregation, Dependency Inversion
- **DRY**: Reuse `infra/numerical-toolbox/` PID, filters, and transforms — do not duplicate
- **Constructor injection**: all hardware dependencies via `PlatformFactory` / constructor, never global state
- **Error handling**: `std::optional<T>` for nullable returns; return error codes/enums in embedded/runtime code (no exceptions); `assert()` for debug preconditions

## Available Agents

Use the agents in `.claude/agents/` for structured development workflows:

- **orchestrator** — Triage and route incoming development tasks
- **planner** — Create detailed implementation plans before writing code
- **executor** — Implement code changes following all project constraints
- **reviewer** — Review code changes against project standards
