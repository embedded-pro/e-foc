# e-foc — Agent Rules

<!-- shared:begin 0.6.0 sha256:f57badde9903d1ff -->
Canonical rules for Claude, Copilot and sub-agents. Sections marked *(shared)* come from embedded-pro/embedded-agent-kit:
edit them there, not here. *(repo)* sections belong to this repository. *Exceptions* overrides a shared rule.

## Behaviour *(shared)*

- **Be as terse as possible.** No preamble, summary or narration. Report file paths and pass/fail.
- Everything you write — code, docs, agent files, PR text — uses the fewest words that stay unambiguous.
- Prose (docs, comments, agent files, commit and PR text) follows ASD-STE100 Simplified Technical English:
  one word, one meaning; active voice; one instruction per sentence; sentences ≤ 20 words (procedures) or ≤ 25 (descriptions).
- Ambiguous request: ask. Clear request: act.
- Do only what was asked. No unrelated refactors, no extra features.
- Batch reads, don't re-read, prefer targeted edits.
- Never change a shared section. Add a deviation to *Exceptions* only when the user approves it.

## Workflow *(shared)*

1. Multi-file or architectural change: update the design document first. Get approval before you write code.
2. Read *Domain* and *Build*. Read each `.github/instructions/*.instructions.md` whose `applyTo` matches a file you change.
3. Find the nearest existing pattern. Follow it.
4. Per behaviour: write a failing test, write the minimum code that passes it, refactor with all tests green.
5. Add each new file to its `CMakeLists.txt`.
6. Update the documentation for each behavioural change.
7. Run the *Build* commands until build and tests pass. Never disable, skip or weaken a test to get green.
8. Non-trivial change: run the `reviewer` agent, or ask the user to run it. Fix each CRITICAL and WARNING finding.
9. Report changed files, build pass/fail, test pass/fail.

## Memory *(shared)*

Scope: production code. Tests and host tools may use the heap.

- Forbidden: `new`/`delete`/`malloc`/`free`, `make_unique`/`make_shared`,
  `std::vector`/`string`/`deque`/`list`/`map`/`set`, recursion.
- Use: `infra::BoundedVector<T>::WithMaxSize<N>`, `infra::BoundedString::WithStorage<N>`,
  `infra::BoundedDeque`, `infra::BoundedList`/`IntrusiveList`, `std::array`, `std::optional`, `std::variant`.

## Interfaces & errors *(shared)*

- No virtual destructors: each adds a deleting destructor and thunks to flash.
  Interface = pure virtual methods + `protected: ~I() = default;` (non-virtual).
- Public `virtual ~Base() = default;` only on a base destroyed through a base pointer:
  `delete`/`std::unique_ptr<Base>`, `infra::PolymorphicVariant<Base, ...>`, `infra::OptionalForPolymorphicObjects<Base, ...>`.
  Put it on that base, not on every interface.
- Not needed for `std::variant`, `std::optional<Concrete>`, `infra::SharedPtr`: they destroy the concrete type.
- No exceptions. `std::optional`, `bool` or status enum; `really_assert` for preconditions. Never swallow an error.
- Constructor injection; no global state.

## Optimisation *(shared)*

- No `#pragma GCC optimize`, no `optimize` attribute: GCC won't inline across differing options.
  The consumer sets flags per translation unit.
- `OPTIMIZE_FOR_SPEED` only on hot functions defined in a header.
- Hot paths: no heap, no virtual dispatch, no blocking.

## Style *(shared)*

- `.clang-format` is authoritative. Allman, 4 spaces, `{}` init.
- PascalCase types/methods; camelCase members and enum values; lowercase namespaces.
  No `I` prefix; implementations end in `Impl`.
- `#pragma once` only.
- Fixed-width integers. `const`/`constexpr`-correct. Functions ≤ 30 lines (hard limit 50).
- Comments: non-obvious *why* only, one line. No `TODO`/`FIXME`, commented-out code, or restating the code.
  Fix or delete a comment the change makes stale.

## Testing *(shared)*

- `TEST_F` for fixtures, `TYPED_TEST` for types, plain `TEST()` for stateless checks.
- `StrictMock` only. `EXPECT_NEAR` with explicit tolerance. Fixture in an anonymous namespace, macros outside.
- Heap allowed in tests.

## Documentation *(shared)*

- Update docs before or with any behavioural change.
- One fact, one place: link, never restate.
- Visuals: Mermaid or ASCII.

## Desktop tools *(shared)*

Scope: host tools that people install and run, GUI or console.

- Open the log file first in `main`, before the application object.
  A GUI-subsystem build (`WIN32_EXECUTABLE`) has no stderr: the log file is its only output.
- Write the log to a per-user writable folder (`AppLocalDataLocation`) and rotate it.
  Capture the toolkit's messages (`qInstallMessageHandler`) from debug level.
- Install a crash handler after the log. It writes a `FATAL` line on a crash signal, `std::terminate`
  or an unhandled SEH exception, and a minidump on Windows. Qt tools use ui-cpp's `IssueLog` and `CrashReporter`.
- Log each start-up step: version, OS, toolkit version, arguments, restored settings, first frame.
- Provide `--safe-start`: it skips the saved state, so a bad saved state cannot block start-up.
- Give the log folder a menu item (Help ▸ Open Log Folder).
- Install test: start the tool, close it normally, then start it again with saved state.
  Print the log. On a failure, upload the log folder and the dumps.
<!-- shared:end -->

## Domain *(repo)*

- Field-oriented control (FOC) for BLDC/PMSM motors on embedded MCUs, with strict real-time and memory limits.
  Includes are rooted at the repository root.
- Architecture:
  - `core/foc/interfaces/` — FOC vocabulary and contracts (`Signals.hpp`, `Foc.hpp`). No algorithms. Unit types
    (`Units.hpp`), parameter ranges (`CommandLimits.hpp`) and their checks (`ParameterValidation.hpp`) come from
    `infra/e-foc-protocol/` (`e-foc-protocol/foc/…`).
  - `core/foc/transforms/` — Clarke/Park transforms, SVM (output straight to `PhasePwmDutyCycles`).
  - `core/foc/math/` — `CoggingFeedForward.hpp`, `TravelLimits.hpp` (header only). Table sine and cosine, angle
    wrapping and finiteness come from the toolbox (`math::FastSine`, `math::FastCosine`, `math::WrapToPi`,
    `math::IsFinite`).
  - `core/foc/cascade/` — cascade orchestration, gain design, per-tick estimator windows. No hardware dependency.
  - `core/foc/model/` — host and QEMU only. Simulation plant (`ThreePhaseMotorModel` from `ElectricalPlant`,
    `MechanicalPlant`, `ThermalModel`, `SupplyModel`, `SimulatedEncoder` over `EncoderModel`, `PhaseCurrentSensor`,
    each through an accessor) and SIL instrumentation (`OneShotDelay`, `PlantResponseRecorder`).
    `drive/`: plant description (`SilPlantConfig`), `PlantDrive` (steps the plant once per control period for QEMU),
    model-in-the-loop tests, virtual motor (`PlantEvents`), `BoardProtectionSimulator`. `memory/`: NVM image
    builder (`BuildNvmImage`) and plant calibration (`CalibratedFor`), shared by harnesses and the virtual motor.
  - `core/foc/instantiations/` — concrete wiring (`Runner`, `FocController`).
  - `core/platform_abstraction/` — `PlatformFactory` and hardware ports (`drivers::ThreePhaseInverter`,
    `drivers::Encoder`) in `interfaces/Drivers.hpp`; `LowSideShuntWindow.hpp` for low-side shunt boards.
  - `core/state_machine/` — motor lifecycle FSM (`Idle` → `Calibrating` → `Ready` ⇄ `Enabled`, `Fault`) on EmIL
    `services::TableStateMachine`: `std::variant` states and events (`FocStateMachineEvents.hpp`), table in
    `FocLifecycleTable`. It registers no terminal commands: `sync_foc_sensored` is commanded over CAN only; only
    `hardware_test` has a command line.
  - `core/services/` — alignment, CLI, system ID, NVM, standstill, current-offset measurement, inverter compensation
    (offsets, dead time), cogging compensation, `fault_history/`, `injection_current_limit/`, multi-turn position
    tracker (`position_tracking/`), status LEDs (`leds/`).
  - `core/can/` — firmware side of the FOC motor CAN category: `FocMotorCanBridge` (server observer), settings,
    current streamer, liveness watchdog, command timeout. `FirmwareUpgradeInterlock` holds the drive for can-lite's
    `services::FirmwareUpgradeSession` (upgrade pack download and staging).
  - `core/supervision/` — runtime plausibility monitors (`EncoderPlausibilityMonitor`).
  - `targets/` — apps (`hardware_test`, `sync_foc_sensored`, `bootloader`) and platforms (`host`, `qemu`, `ti`,
    `st`, `cortex_m_common`, `error_handling_cortex_m`, `motor_boards`, `peripheral_stubs` for host/ST).
  - `infra/numerical-toolbox/` — PID, filters, fixed-point math (see its `AGENTS.md`).
  - `infra/e-foc-protocol/` — FOC motor CAN wire contract (`e-foc-protocol/contract/`), category server and client,
    `FocMotorCanClient`. A contract change starts there (its `documentation/design/wire-contract.md`), then bumps
    the submodule here.
  - `infra/embedded-infra-lib/` — bounded containers, build helpers, toolchain cmake, upgrade pack components and
    the `bootloader` package (status journal, header and descriptor checks, installer, boot sequence, key slot),
    with the `application.bootloader_image` host tool and its CMake steps (`documentation/design/bootloader.md`
    Part J).
  - `tools/virtual_motor/` (`documentation/design/virtual-motor.md`): `platform/` virtual board (`VirtualPlatform`
    over `PlantDrive`, memory file, `virtual:` CAN port, wall-clock `Pacer` under a pausable `RunLoop`,
    `PlantDisturbances` for live torque and noise, motor side of the plant port); `port/` plant-port frames and
    messages, shared with the app; `engine/` headless `e-foc-virtual-motor` that runs `Logic`; `app/` the e-foc
    Virtual Motor app (`documentation/design/tool-virtual-motor.md`): `logic/` motor processes, plant ports, trace
    history, captures, trace/modulation/rotor/encoder views; `gui/` manager and motor windows.
  - `tools/can_commander/` — CAN commander GUI, issue log and update check (`support/`, over ui-cpp's
    `ui/backend/qt/update`; the firmware-pack lookup stays here), installers (`installer/`).
  - `tools/hardware_bridge/` — terminal/CAN/serial bridge client for the HIL runner.
- *Memory* covers `core/foc/`, `core/platform_abstraction/`, `core/state_machine/`, `targets/` and every
  ISR-reachable path. `tools/` are host tools.

### Real time

- `Calculate()` runs at 20 kHz in an interrupt. Budget at 120 MHz: inner loop ≤ 4500 cycles (75 % of 6000);
  1 kHz outer loop ≤ 20000.
- Hot path: no virtual dispatch, no heap, no raw `sin`/`cos`. Use `math::FastSine`/`math::FastCosine`
  (`numerical/math/FastTrigonometry.hpp`) or a lookup table. No blocking call in ISR-reachable code.
  `Calculate()` delegates to focused helper methods.
- No validation per sample: no plausibility check on phase currents, no finiteness check on currents or duty cycles
  in the 20 kHz path. Sensor and power-stage failures belong to the hardware protection path.
- `math::IsFinite` (`numerical/math/Math.hpp`) is for configuration-time and outer-loop checks only
  (`CurrentPlantModel::IsUsable`, the mechanical estimator's plausibility band), never in `Calculate()`.
- A function defined in a `.cpp` never carries `OPTIMIZE_FOR_SPEED`: other translation units would call an
  `always_inline` function without its body, and the build fails. With `NUMERICAL_TOOLBOX_ENABLE_OPTIMIZATIONS`
  (embedded and QEMU presets) the macro is `always_inline` + `inline`; else it is empty.
- A free function defined in a header takes `ALWAYS_INLINE_HOT`, not `OPTIMIZE_FOR_SPEED`: it must stay `inline`
  when the macro is empty, and the macro's own `inline` would duplicate it.
- Embedded and QEMU builds compile all of e-foc with the same options (see *Build*, Optimisation).

### FOC theory

- Clarke: `Iα = (2/3)·(Ia − (Ib+Ic)/2)`, `Iβ = (Ib − Ic)/√3` (amplitude-invariant, 3 phases).
- Park: `Id = Iα·cos(θ) + Iβ·sin(θ)`, `Iq = −Iα·sin(θ) + Iβ·cos(θ)`.
- Electrical angle: `θe = θm · pole_pairs`.
- Anti-windup: every PID integrator clamps or uses back-calculation.
- Decoupling: −ω·Lq·Iq feedforward on Vd; +ω·(Ld·Id + ψf) on Vq. Vd term negative, Vq term positive: they cancel
  the plant coupling; reversed signs double it.
- SVM: min-max common-mode injection; duty cycles bounded to [0, 1].
- Reuse `Clarke`, `Park`, `ClarkePark` (`TransformsClarkePark.hpp`) and `SpaceVectorModulation` from
  `core/foc/transforms/`, and PID, filters and math from `infra/numerical-toolbox/`. Do not reimplement them.
- Math through the `numerical/math/Math.hpp` wrappers (`math::Abs`, `math::Sqrt`, `math::Sin`, `math::Exp`,
  `math::Round`, `math::IsFinite`, ...), never `std::abs`, `std::sqrt` etc.: a target can override each wrapper.
  `std::min`, `std::max`, `std::clamp` stay. Tests may use `std::` as the reference.
- Unit types, never a raw `float` where one exists: `Ampere`, `Radians`, `Volts`, `RevPerMinute`,
  `PhasePwmDutyCycles`, `PhaseCurrents`.
- Theory background: `documentation/theory/foc.md`.

### Code structure

- `public:` before `private:`. Non-trivial logic in `.cpp`; small `inline`/`constexpr` helpers in headers.
- Interfaces: a concrete class declares no destructor or a plain public one.
- Constructor injection through `PlatformFactory` and the ports in `core/platform_abstraction/interfaces/Drivers.hpp`;
  depend on abstractions. One class, one control concern; RAII for resources.
- A new FOC mode implements `FocTorque`, `FocSpeed` or `FocPosition` (`core/foc/interfaces/Foc.hpp`) and every
  pure virtual, `FocBase`'s included.

### Async callback lifetime

A service that schedules event-dispatcher closures and lives in an FSM `std::variant` follows this pattern
(`documentation/design/async-callback-lifetime.md`):

1. Inherit `infra::EnableSharedFromThis<T>`.
2. Schedule through `infra::EventDispatcherWithWeakPtr::Instance().Schedule([](const infra::SharedPtr<T>& self){ … },
   WeakFromThis())`. Raw `[this]` captures on the dispatcher are forbidden.
3. Wrap each construction site in `infra::WithSharedAccess<T>` (`infra/util/WithSharedAccess.hpp`), never a bare
   member or local.
4. Expose `IsRunning() const` on the service interface; `true` while estimation state is active.
5. The owning `FocStateMachine` subclass ORs `service.IsRunning()` into `HasPendingAsyncWork()`. Without it, a mode
   switch proceeds while hardware is still driven.
6. A fixture holding a `WithSharedAccess` member calls `ExecuteAllActions()` in `TearDown()`. Each `StrictMock`
   service mock gets `EXPECT_CALL(…, IsRunning()).WillRepeatedly(Return(false))` in the teardown setup.

Static objects (application composites never destroyed while the event loop runs) are exempt from rules 1–3.

### Testing

- Test files: `test/Test{ComponentName}.cpp` in the library under test, added with `add_subdirectory(test)` in that
  library's `CMakeLists.txt`.
- Host stubs: `targets/platform_implementations/host/`.

### Documentation

- `documentation/theory/` — FOC algorithm or motor model changes.
- `documentation/performance-optimization/README.md` — timing-critical changes.
- `documentation/architecture/`, `documentation/design/` — a component whose observable behaviour changes. No
  document yet: create one from `documentation/templates/architecture.md` or `design.md` before production code.
- `documentation/verification_and_validation/` — test plans, test design (integration, SIL) and product
  requirements (`requirements/*.yaml`). A test strategy is never a design document.
- Visuals: Mermaid (flowcharts, state, sequence), TikZ `{=latex}` raw blocks (technical charts, signal flows,
  coordinate frames; source in `documentation/tikz/`) or ASCII.
- Image syntax only for: the pre-rendered TikZ preview (`documentation/tikz/images/*.svg`) next to its `{=latex}`
  block; in `documentation/theory/` only, an animated figure from `documentation/animations/`.
- After editing `documentation/tikz/*.tex`, run `python documentation/tools/render-tikz-svgs.py`.
- `python documentation/tools/generate-animations.py` generates every `documentation/animations/*.svg` (SMIL plus a
  `-frames.svg` strip for the PDF). Edit the script, never the SVGs; commit both. CI runs it with `--check`.
- Every document has front matter (`title`, `type`, `status`, `version`, `component`, `date`). The metadata table
  and template notes after it are for GitHub readers; the PDFs omit them and print the front matter on the title
  page.
- A design document opens with `## Responsibilities`: one list. No "Is NOT responsible for" list.
- No "Open Questions" section: an undecided item is a GitHub issue; a known limitation goes in
  *Constraints & Limitations*.
- Design and architecture documents describe the component, not its hardware: no MCU or board names, vendor
  peripherals, registers or clock frequencies. Say "the target"; "on the reference board" for a measured figure.
- PDFs: `documentation/tools/build-foc-booklet.py` builds the booklet and one PDF per document
  (`--format pdf documents`). Each opens with title page (title, author, date), blank page, contents, blank page,
  body. A new folder gets an entry in `DOCUMENT_FOLDERS`; a new chapter its line in
  `documentation/booklet/README.md`.
- Build the PDFs after a documentation change and check the result: every Mermaid diagram renders (a failure fails
  the build); no table column runs into the next. Keep inline code in table cells short.
- The release ships `e_foc-documentation-<tag>.zip`: `eFocDesign.pdf` at the root; document PDFs in `design/`
  (architecture included), `theory/`, `verification_and_validation/` (requirements and traceability matrix
  included).

### Requirements and scenarios

- Scenarios live in `integration_tests/features/`, tagged by runner: `@sil` (QEMU SIL), `@hil` (hardware),
  `@virtual` (virtual motor runner, `--mode virtual`, host `e-foc-virtual-motor`; a small functional set that keeps
  `@sil`, run by `ci.yml`'s `virtual_motor_tests`).
- Every SIL scenario runs under `@sil`; no held-out tag. Fix a defect a scenario exposes at the root cause, never
  park it (`documentation/verification_and_validation/software-in-the-loop.md` Parts E, F, I, J).
- Two requirement tiers (`documentation/verification_and_validation/integration-testing.md`,
  *Feature-to-Requirements Mapping*): `requirements/*.yaml` holds product requirements — solution-free promises
  observable at the drive's boundary, one per `shall`, no class or message names, no contract versions, no per-law
  limit tables. Scenario `Examples:` tables hold acceptance limits.
- A mechanism or design constraint goes in its design document, never in a requirement. A limit pinned from a
  `[METRIC]` line goes in the scenario only.
- Each requirement has a `status` (`draft`, `implemented`, `verified`, `retired`) and a `verification` (`sil`,
  `hil`, `mil`, `unit`, `inspection`). A `verified` requirement must be cited by an `@REQ-...` tag.
- `documentation/tools/check-requirements-traceability.py` (`requirements-validation.yml`) fails on: missing
  status or verification, a `shall` over 90 words or naming another requirement, a `verified` requirement with no
  tag, a tag naming an unknown or retired ID, a duplicate ID, a scenario kind the `verification` field omits.
- Model-in-the-loop (`integration_tests/model_in_the_loop`) composes the control-mode FSM, calibration services and
  NVM stack as `Logic` does, minus CAN, against the same plant, stepped on the host at the control rate. Sweeps over
  laws, plants, torque signs and seeds go there. It is a gtest binary that `ctest --preset host` runs. A change to
  how `Logic` wires a service is made in `ModelInTheLoop` too (`software-in-the-loop.md` Part N).
- SIL: the plant is an input to the target. The harness writes a plant description and an NVM image into a
  per-scenario working directory; the firmware reads both by relative name
  (`documentation/verification_and_validation/software-in-the-loop.md`).
- Each SIL scenario boots through the emulated bootloader, loaded with the public development key set;
  `staging.bin` and `journal.bin` are its staging slot and status journal (Part P).
- `SIL_VERBOSE=1` traces both directions and keeps the scenario directory. `SIL_GDB=1` starts the emulator halted
  with a GDB stub on port 1234.
- SIL performance: the plant writes `PLANT` lines to `plant.rec` in the scenario directory (control-tick time base,
  never from the ISR). A shaft torque step is scheduled in the plant description.
  `integration_tests/support/response/` measures step and disturbance metrics with
  `numerical/math/StepResponseMetrics.hpp`.
- Each metric assertion prints a `[METRIC]` line. Pin limits from those lines with a margin, never from theory
  alone.
- Reference motors: `motor_parameters/` (Teknic M-2310P-LN-04K, Anaheim BLY172S-24V-4000; sources in
  `documentation/theory/foc-plant-models.md` "Reference motors"). Plant presets (`motor_parameters/PlantPresets.hpp`)
  `teknic`, `anaheim`, `nominal` (= Teknic) select them for SIL, MIL and the virtual motor.
- Identification scenarios (`parameter_identification.feature`) compare what the firmware identifies (CAN
  electrical response, calibration record, online estimates read as contract 2.2 parameters) with the configured
  plant.
- SIL channels: CAN frames are `CAN_RX`/`CAN_TX` text lines on the emulator's second serial port. The first is the
  product terminal; no scenario writes to it. Firmware output is the scenario's `debug.log`; a failed scenario's
  report carries its last lines.
- Terminal guard: motor-controller steps use `integration::Fixture`, CAN only. The terminal is
  `TerminalInteractor` (hardware interactor only) behind `TerminalFixture`, emplaced by the `@hil` bring-up hook.
  Never add terminal or trace access to `Fixture` or `TargetInteractor`. Tag motor-controller scenarios on hardware
  `@hil_motor`, never `@hil`: only `@hil` scenarios get the terminal.

### Agents and skills

- `analyst` (`.claude/agents/`, `.github/agents/`) — investigation, audit, root-cause analysis, design exploration,
  CI/build/test failure diagnosis. Read only.
- `reviewer` — review against this file.
- Skill `ux-ui-expert` (`.claude/skills/ux-ui-expert/SKILL.md`) — load it before you design, review or change
  anything a person sees or operates under `tools/`: layouts, safety controls, feedback, installers, update and
  issue-reporting flows.

## Build *(repo)*

```bash
cmake --preset host && cmake --build --preset host-Debug
ctest --preset host
cmake --preset EK-TM4C1294XL && cmake --build --preset EK-TM4C1294XL-Debug
cmake --preset qemu-foc-sensored && cmake --build --preset qemu-foc-sensored-RelWithDebInfo
```

Presets: `host`, `host-single-Debug`, `coverage`, `EK-TM4C1294XL`, `NUCLEO-G474RE`, `NUCLEO-H563ZI`,
`qemu-foc-sensored`.

Before you push, verify locally (CI minutes are finite):

```bash
cmake --preset host && cmake --build build/host --config Debug -j$(nproc)
ctest --test-dir build/host -C Debug -E "e_foc.integration_tests" --output-on-failure
cmake --preset qemu-foc-sensored && cmake --build build/qemu-foc-sensored \
  --target e_foc.bootloader.main e_foc.sync_foc_sensored.main -j$(nproc)
QEMU_SIL_ELF=build/qemu-foc-sensored/targets/sync_foc_sensored/main/RelWithDebInfo/e_foc.sync_foc_sensored.main.elf \
  build/host/integration_tests/main/Debug/e_foc.integration_tests \
  --mode sil integration_tests/features/ -t "@sil"
```

- The `defaults` test preset excludes the `hardware|integration` labels: `ctest --preset host` does not run the
  integration suite. Run it directly, as above.
- CMake: add each new file to its library's `CMakeLists.txt`. Tests come in through `add_subdirectory(test)`.
- Embedded CMake: call `halst_target_bringup(<target>)` (ST) or `hal_ti_target_bringup(<target>)` (TI) in
  `targets/*/main/CMakeLists.txt`. The `*_default_init` variants are removed.
- Bootloader keys: no key is a build input. The bootloader builds with an erased key slot that refuses every pack.
  EmIL's `application.bootloader_image inject-keys --keys <set> --elf <bootloader elf> --hex <file>...` then
  writes the slot. An embedded build builds the tool at `build/<preset>/_native_bootloader_image/bin`
  (`bootloader.md` Part J).
- The development key set `targets/bootloader/keys/development.keys` is public: CI, dry runs and local builds
  inject it. The release set is the only secret, `BOOTLOADER_KEYS_RELEASE` (base64), injected by the release only
  (`bootloader.md` Part G).
- On a board with a bootloader (EK-TM4C1294XL), `sync_foc_sensored` links into the application slot. The build
  stamps its image descriptor with `emil_bootloader_image_descriptor` and `application.bootloader_image`. A debugger
  programs two images: `e_foc.bootloader.main.hex` and `e_foc.sync_foc_sensored.main.hex`. `hardware_test` links
  standalone at the start of flash on every board and replaces the bootloader.
- `qemu-foc-sensored` builds `e_foc.bootloader.main` and `e_foc.sync_foc_sensored.main` for emulated Cortex-M4 via
  semihosting, laid out like a board with a bootloader. The application ELF with `.image.bin` and
  `.descriptor.bin`, and the bootloader ELF, are CI artifacts for the SIL shards.
- Cycle budget gate: the `cortex-cycle-budget` static analysis in `ci.yml`'s embedded build. EK-TM4C1294XL at
  120 MHz: inner ≤ 4500, outer ≤ 20000, identification ≤ 6000. NUCLEO-G474RE at 170 MHz: 6375 / 28333 / 8500.
  Each MCU gets one cycle banner and one memory banner on the PR.
- The analysis charges each path its worst case through the named functions and their direct calls. It does not
  follow a call through a pointer (`infra::Function`, virtual). Name the target of a new one on a hot path in
  `targets/sync_foc_sensored/main/cycle-analysis.json` (inner), `cycle-analysis-outer.json` (outer) or
  `cycle-analysis-identification.json` (inner plus calibration observer).
- Alternatives of which one runs share a stage with `"combine": "max"`. A new loop needs a `loop_bounds` entry. A
  change to the loops of a function whose entry is a list (one bound per loop, in address order) rewrites the list.
  After a hot-path change, read the report's *Analysis Notes* (calls not followed, loops without a bound).
  `CYCLE_BUDGET_PACKAGE` in `ci.yml` pins the analyser; rules in `documentation/performance-optimization/README.md`
  (*What the static estimate charges*).
- Warnings: `emil_enable_warnings()` (`infra/embedded-infra-lib/cmake/emil_compiler_warnings.cmake`) enables
  `-Wall -Wextra` minus GCC/Clang false positives on EmIL and GoogleTest. With `CMAKE_COMPILE_WARNING_AS_ERROR`,
  any warning in `core/`, `targets/`, `integration_tests/` or `tools/` fails the build.
- Every submodule `add_subdirectory` and every `FetchContent_Declare` passes `SYSTEM`; a new dependency does too.
  The warnings call sits after the dependencies and before `add_subdirectory(core)`.
- Never silence a warning with a pragma. Fix it, or drop the name of a parameter that is not used.
- `-Wmaybe-uninitialized` is off, for GNU only: GCC inlines `infra::Function::operator()` and reports false paths
  against the caller. Definite `-Wuninitialized` stays on. The GNU guard matters: Clang has no such flag, and
  `-Wunknown-warning-option` would fail the build.
- Optimisation: `numerical_toolbox_enable_embedded_optimizations()`
  (`infra/numerical-toolbox/cmake/NumericalCompilerOptions.cmake`), called when `E_FOC_EMBEDDED_BUILD` is on, adds
  `-ffast-math -fno-finite-math-only` in embedded and QEMU builds; `-fno-finite-math-only` keeps NaN and infinity
  checks meaningful. The call sits next to the warnings call, so `core/` and `targets/` get the options and
  infrastructure libraries and HALs keep their own.
- Debug level: `-Og` on the host, `-O1` with `E_FOC_EMBEDDED_BUILD` (an `-Og` image overflows the EK-TM4C1294XL
  application slot), set in the root `CMakeLists.txt`. EmIL's ARM toolchain defines `__ASSERT_FUNC=nullptr` for C++
  (`documentation/performance-optimization/README.md`, *Debug Builds*).
- Warning and optimisation settings never go in `CMakePresets.json`: a preset condition cannot branch on
  `CMAKE_CXX_COMPILER_ID`, and both guards are load-bearing.
- ccache: every configure preset inherits `defaults`, which sets `ccache` as compiler launcher as cache variable and
  in the environment (so `_native_bootloader_image` is cached too), with `CCACHE_BASEDIR` at the source tree. Each
  compiling CI job restores a cache with `hendrikmuhs/ccache-action` (saved from `main` only). A job that configures
  without a preset passes the launcher itself. A new compiling job does the same.
- CI: `ci.yml` builds all targets and uploads `e_foc`, `e_foc.integration_tests`, `e_foc.virtual_motor`,
  `e_foc.sync_foc_sensored.qemu.elf`, `e_foc.bootloader.qemu.elf`. Its `release_tools_build` job calls
  `build-tools.yml` (shared with the release): every PR builds, packages and install-tests the Linux `.deb` and
  Windows tools (MSVC, warnings as errors, release Qt). Signing is release only.
- The three `software_in_the_loop_shard` jobs (`needs: [host_build_test_ubuntu, qemu_foc_sensored_build]`) download
  the artifacts and run behavioural tests; they never compile. Do not add cmake build steps to them.
- Each shard runs the features `integration_tests/sil-shards.json` assigns. `integration_tests/sil-shard.py` checks
  that each feature with an `@sil` scenario is listed exactly once. Put a new feature in the shard that keeps the
  three balanced. `software_in_the_loop_tests` merges the reports into the `Software-in-the-Loop Test Results`
  check; a re-run shard replaces its report.
- Other PR workflows: `linting-formatting.yml` (MegaLinter), `static-analysis.yml` (SonarCloud),
  `requirements-validation.yml`, `submodule-check.yml` (submodules on `main`), `agents.yml`; and
  `documentation-validation.yml` and `build-booklet.yml` when `documentation/` changes.
- Release: `release-please.yml` builds through `build-tools.yml` and attaches the Linux/Windows tools (CAN Commander
  `.deb` and NSIS `setup.exe`, each installed, launched and uninstalled first), one firmware archive per board
  (`e_foc-firmware-<board>-<tag>.tar.gz`: each application's `.elf` and `.hex` named
  `e_foc-firmware-<board>-<application>-<tag>`, and the board's signed `.upgrade` pack), the pack as a standalone
  asset, `e_foc-source-<tag>.tar.gz` with every submodule, and `e_foc-documentation-<tag>.zip`.
- Each release asset gets a Sigstore build-provenance attestation. The release verifies the installers', the
  EK-TM4C1294XL archive's and the pack's before upload. SignPath Authenticode-signs the CAN Commander executable and
  installer once `SIGNPATH_ORGANIZATION_ID` is set (`.github/signpath/README.md`).
- Keep asset-name suffixes stable: the CAN Commander update check finds `-linux-x86_64.deb`,
  `-windows-x86_64-setup.exe` and `e_foc-firmware-<board>-sync_foc_sensored-<tag>.upgrade`.
- After a workflow change, dispatch the release with `dry_run: true`: it builds and checks every asset with the
  development keys and creates no release. A real release fails without `BOOTLOADER_KEYS_RELEASE`.
- `release-please-config.json` lists `feat`, `fix`, `perf`, `revert`, `docs`, `build`, `ci` in the changelog; any of
  them opens or updates the release PR. `refactor`, `test`, `style`, `chore` stay hidden.

## Exceptions *(repo)*

- Comments: also allowed for a unit, frame or convention a type does not carry (electrical vs mechanical, per-unit
  vs volts, per-sample vs continuous gain) and for a concurrency contract (which context writes, which reads).
  Reason: the FOC loop mixes frames and ISR/main contexts.
- Comments: no section banners, no docstrings on clear methods, no comment that describes the change for a
  reviewer, no `HACK` or `XXX` (raise an issue). A comment never claims behaviour the code does not have, nor, in a
  test, a check it does not make. Reason: stale or false comments mislead more than none.
- Visuals: TikZ `{=latex}` blocks and the image references in *Documentation* are allowed besides Mermaid and ASCII.
  Reason: the booklet PDFs need technical charts and coordinate frames.
