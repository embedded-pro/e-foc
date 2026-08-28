# QEMU SIL — Full Behavioral Test Suite on Emulated Hardware

**Branch:** `feature/qemu-sil`
**Depends on:** commits already on this branch (CMake presets, QemuSilSession/Fixture, SilQemuMain, FocCycleBudgetSteps)

---

## Goal

Run **all** `integration_tests/software_in_the_loop/features/` cucumber scenarios against FOC code executing on QEMU (emulated Cortex-M4/M7 via semihosting), not on the host.

The existing `.feature` files and their `@REQ-*` requirement tags are reused without modification.  New QEMU-specific step definitions drive the full motor control stack over semihosting I/O.

---

## Architecture

```
Host (Linux process)                         QEMU (ARM emulator)
─────────────────────────────────────────    ──────────────────────────────────────
e_foc.qemu_sil_tests (new executable)        e_foc.sil_qemu (ELF, extended)
  cucumber runner reads features/              ├─ infra::EventDispatcherWithWeakPtr
  │                                            ├─ SemihostingCan (new)
  └─ QemuSilSession (existing, extended)       │    reads/writes CAN_RX/TX lines
       stdin  ──→  CAN_RX <id> <hex>  ──→     ├─ TorqueStateMachine
       stdout ←──  CAN_TX <id> <hex>  ←──     ├─ SpeedStateMachine
              ←──  SERIAL <text>      ←──     ├─ PositionStateMachine
              ←──  DWT key=NNN        ←──     ├─ CanProtocolServer
              ←──  READY / DONE       ←──     ├─ FocMotorCategoryServer
                                               └─ SilQemuPlatformStub (new)
```

### Semihosting line protocol (extends current READY/DWT/DONE)

| Direction | Format | Meaning |
|---|---|---|
| QEMU → host | `READY` | Target initialised, event loop running |
| QEMU → host | `CAN_TX <id_hex> <data_hex>` | CAN frame sent by motor controller |
| Host → QEMU | `CAN_RX <id_hex> <data_hex>` | Inject CAN frame into motor controller |
| QEMU → host | `SERIAL <text>` | Tracer/terminal output (informational) |
| QEMU → host | `DWT key=NNN` | Cycle-count measurement result |
| QEMU → host | `DONE` | Command complete |
| Host → QEMU | `perf` | Run cycle benchmark |
| Host → QEMU | `quit` | Terminate |

`id_hex` = up to 3 hex digits (11-bit CAN ID).  
`data_hex` = up to 16 hex chars (8 bytes, big-endian).

---

## File plan

### New files

| File | Purpose |
|---|---|
| `integration_tests/software_in_the_loop/qemu_target/SemihostingCan.hpp` | `hal::Can` backed by semihosting stdin/stdout |
| `integration_tests/software_in_the_loop/qemu_target/SemihostingCan.cpp` | Implementation |
| `integration_tests/software_in_the_loop/qemu_target/SilQemuApplication.hpp` | Full motor stack wiring (mirrors `sync_foc_sensored` Logic) |
| `integration_tests/software_in_the_loop/qemu_target/SilQemuApplication.cpp` | Implementation |
| `integration_tests/software_in_the_loop/steps/QemuStateMachineSteps.cpp` | State machine scenarios via CAN |
| `integration_tests/software_in_the_loop/steps/QemuCanSteps.cpp` | CAN protocol scenarios |
| `integration_tests/software_in_the_loop/steps/QemuCalibrationSteps.cpp` | Calibration flow scenarios |
| `integration_tests/software_in_the_loop/steps/QemuSpeedSteps.cpp` | Speed controller scenarios |
| `integration_tests/software_in_the_loop/steps/QemuPositionSteps.cpp` | Position controller scenarios |
| `integration_tests/software_in_the_loop/qemu_main/CMakeLists.txt` | Builds `e_foc.qemu_sil_tests` (host executable) |

### Modified files

| File | Change |
|---|---|
| `integration_tests/software_in_the_loop/qemu_target/SilQemuMain.cpp` | Add `--benchmark` argv mode; construct `SilQemuApplication` in interactive mode |
| `integration_tests/software_in_the_loop/qemu_target/CMakeLists.txt` | Add `SemihostingCan`, `SilQemuApplication`; link state machine + CAN libs; update ctest to pass `--benchmark` |
| `integration_tests/software_in_the_loop/support/QemuSilSession.hpp/cpp` | Add `SendCanFrame()`, `WaitForCanFrame()` |
| `integration_tests/software_in_the_loop/support/QemuSilFixture.hpp/cpp` | Add `SendCanFrame()`, `WaitForCanFrame()`, `SendCommand()` |
| `integration_tests/software_in_the_loop/CMakeLists.txt` | Add `qemu_main/` subdir |
| `.github/workflows/ci.yml` | Run `e_foc.qemu_sil_tests` in the host CI job after downloading the QEMU ELF artifact |

---

## Phase 1 — `SemihostingCan`

Implements `hal::Can` using semihosting stdin/stdout.

```cpp
// SemihostingCan.hpp
class SemihostingCan
    : public hal::Can
{
public:
    void SendData(Id id, const Message& data,
                  const infra::Function<void(bool)>& onDone) override;
    void ReceiveData(const infra::Function<void(Id, const Message&)>& onReceived) override;

    // Called from the main loop each iteration — non-blocking stdin read
    void PollIncoming();

private:
    infra::Function<void(Id, const Message&)> onReceived;
};
```

**`SendData`**: formats `CAN_TX <id_hex> <data_hex>\n` to `stdout`, calls `onDone(true)` synchronously (no ACK wait needed for SIL).

**`ReceiveData`**: stores the callback (only one receiver, same as `hal::Can` contract).

**`PollIncoming`**: calls `fgets` with `O_NONBLOCK` on `stdin` (set via `fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK)`); parses `CAN_RX <id_hex> <data_hex>` lines; dispatches to `onReceived`. Returns immediately if nothing is available.

Encoding: 11-bit CAN ID as 3 hex digits; data as up to 16 hex chars (0-padded to even length).

---

## Phase 2 — `SilQemuApplication`

Mirrors `targets/sync_foc_sensored/main/instantiations/` but wired to semihosting:

```cpp
// SilQemuApplication.hpp
struct SilQemuApplication
{
    infra::EventDispatcherWithWeakPtr::WithSize<50> eventDispatcher;

    SemihostingCan can;
    SilQemuPlatformStub platformStub;   // see below

    // NVM backed by a fixed array (same as EepromStub in host SIL)
    std::array<uint8_t, 256> eepromStorage{};
    // ... nvm, cascade, state machines, can server wired here

    // Prints READY, then runs: eventDispatcher.Run() interleaved with can.PollIncoming()
    void Run();
};
```

**`SilQemuPlatformStub`** provides:
- `FocLowPriorityInterruptAdapter` (already exists from commit 1 on this branch)
- `hal::PerformanceTracker` no-op implementation
- `hal::Eeprom` backed by the fixed array
- Returns `foc::Volts{ 48.0f }` for bus voltage, `foc::Ampere{ 15.0f }` for max current

**Event loop design**: since `infra::EventDispatcher::Run()` blocks, and we need to poll semihosting stdin for CAN frames between events:

```cpp
void SilQemuApplication::Run()
{
    std::puts("READY");
    std::fflush(stdout);

    while (true)
    {
        eventDispatcher.ExecuteAllActions();
        can.PollIncoming();
    }
}
```

---

## Phase 3 — Refactor `SilQemuMain.cpp`

```cpp
int main(int argc, char** argv)
{
    const bool benchmarkMode = (argc >= 2 && std::strcmp(argv[1], "--benchmark") == 0);

    if (benchmarkMode)
    {
        RunDwtBenchmark();   // existing benchmark code, extracted to function
        return 0;
    }

    SilQemuApplication app;
    app.Run();
    return 0;
}
```

Update `qemu_target/CMakeLists.txt` test command to pass `--benchmark`:

```cmake
# Replace emil_add_test(e_foc.sil_qemu) with a manual add_test that passes --benchmark
add_test(NAME e_foc.sil_qemu COMMAND e_foc.sil_qemu --benchmark)
```

With `CMAKE_CROSSCOMPILING_EMULATOR` set, ctest invokes:
```
qemu-system-arm -M mps2-an386 ... -kernel e_foc.sil_qemu.elf -- --benchmark
```
QEMU passes `--benchmark` via the semihosting `SYS_GET_CMDLINE` mechanism which newlib-nano exposes as `argv`.

---

## Phase 4 — Extend `QemuSilSession` and `QemuSilFixture`

### `QemuSilSession` additions

```cpp
// Sends: "CAN_RX <id_hex> <data_hex>\n"
bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& data);

// Waits for: "CAN_TX <id_hex> <data_hex>\n" matching the given ID
bool WaitForCanFrame(hal::Can::Id expectedId,
                     hal::Can::Message& out,
                     std::chrono::milliseconds timeout);
```

`WaitForCanFrame` reuses `WaitFor("CAN_TX", ...)` and parses the hex payload.

### `QemuSilFixture` additions

Mirror `HilFixture`'s public interface so QEMU step definitions have the same API as HIL steps:

```cpp
bool SendCanFrame(hal::Can::Id id, const hal::Can::Message& message,
                  std::chrono::milliseconds timeout = std::chrono::milliseconds{100});

bool WaitForCanFrame(hal::Can::Id expectedId,
                     hal::Can::Message& outPayload,
                     std::chrono::milliseconds timeout,
                     std::chrono::milliseconds& outElapsed);

bool SendCommand(const std::string& command,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});
```

---

## Phase 5 — QEMU step definitions

Each new step file corresponds to an existing host SIL step file.  **The step text regex must be identical** so the same `.feature` files dispatch to them.

Because cucumber-cpp registers steps globally by regex, having both host and QEMU steps in the same executable would cause conflicts.  The solution is **two separate executables** (Option A):

| Executable | Links | Reads features from |
|---|---|---|
| `e_foc.integration_tests` | host SIL steps + `FocIntegrationFixture` (unchanged) | `features/` |
| `e_foc.qemu_sil_tests` | QEMU SIL steps + `QemuSilFixture` | `features/` |

Both read the same `features/` directory.  The QEMU executable skips (via `GTEST_SKIP`) any scenario that does not have a QEMU step implementation yet.

### Step files and what they need

| New file | Mirrors | Key difference |
|---|---|---|
| `QemuStateMachineSteps.cpp` | `StateMachineSteps.cpp` | Uses `QemuSilFixture::SendCanFrame` / `WaitForCanFrame` for start/stop/fault transitions |
| `QemuCanSteps.cpp` | `CanSteps.cpp` | Thin wrapper — almost identical, swaps `HilFixture` for `QemuSilFixture` |
| `QemuCalibrationSteps.cpp` | `CalibrationSteps.cpp` | CAN calibration frames; no mock alignment service (wire no-op impl in `SilQemuApplication`) |
| `QemuSpeedSteps.cpp` | `SpeedSteps.cpp` | CAN speed setpoint frames; reads back via telemetry frame (see note below) |
| `QemuPositionSteps.cpp` | `PositionSteps.cpp` | Same pattern as speed |

### Telemetry CAN frame (required for functional tests)

`speed_functional` and `position_functional` scenarios check that "commanded duty cycles follow the velocity/position setpoint."  These inspect internal state not normally on CAN.

Add a **telemetry CAN frame** to `SilQemuApplication` that is broadcast after each FOC ISR cycle:
- **CAN ID**: `0x7FF` (or a dedicated debug ID, configurable)
- **Payload**: `[duty_a_u16, duty_b_u16, duty_c_u16, reserved_u16]` (8 bytes)

The QEMU step definitions listen for this frame and assert on the duty cycle values, replacing the in-process `EXPECT_NEAR` checks.

Alternatively, tag scenarios that inspect internal state with `@host-only` and skip them in the QEMU runner — document this explicitly.

---

## Phase 6 — Scenarios that need special handling

| Feature / scenario | Challenge | Recommended action |
|---|---|---|
| `speed_functional` / `position_functional` — duty cycle assertions | Internal state, not on CAN | Add telemetry frame **or** tag `@host-only` |
| `CallbackRaceSteps` | Uses `infra::ClockFixture`, timing dependent | Tag `@host-only`; not meaningful on QEMU |
| `calibration_flow` — mock alignment/ident services | Mocks are host in-process only | Wire no-op real implementations in `SilQemuApplication`; accept that calibration completes trivially |
| `can_control_mode` / `can_foc_motor` | Pure CAN protocol — fully portable | Should work without adaptation |
| `state_machine_lifecycle` | Driven entirely via CAN start/stop — fully portable | Should work without adaptation |

---

## Phase 7 — CMake and CI wiring

### `qemu_target/CMakeLists.txt`

Add sources and links:
```cmake
target_sources(e_foc.sil_qemu PRIVATE
    SemihostingCan.cpp
    SilQemuApplication.cpp
    # existing:
    SilQemuMain.cpp
)

target_link_libraries(e_foc.sil_qemu PRIVATE
    e_foc.foc.cascade
    e_foc.state_machine       # new
    e_foc.can                 # new
    can_lite.server           # new
    hal.cortex_m
)

# Replace emil_add_test with explicit add_test passing --benchmark
add_test(NAME e_foc.sil_qemu COMMAND e_foc.sil_qemu --benchmark)
```

### `qemu_main/CMakeLists.txt` (new)

```cmake
add_executable(e_foc.qemu_sil_tests)
emil_build_for(e_foc.qemu_sil_tests HOST All BOOL E_FOC_BUILD_TESTS)

target_link_libraries(e_foc.qemu_sil_tests PRIVATE
    e_foc.integration_tests.qemu_steps   # new library with QEMU step defs
    e_foc.integration_tests.support      # existing (for QemuSilFixture)
    cucumber_cpp.runner
)

add_test(NAME e_foc.qemu_sil_tests
    COMMAND e_foc.qemu_sil_tests -- ${CMAKE_CURRENT_SOURCE_DIR}/../features)
set_tests_properties(e_foc.qemu_sil_tests PROPERTIES LABELS integration)
```

### `software_in_the_loop/CMakeLists.txt`

Add `add_subdirectory(qemu_main)` alongside `add_subdirectory(main)`.

### `.github/workflows/ci.yml`

Extend `host_build_test_ubuntu`:
1. After the QEMU build job completes, download the `e_foc.sil_qemu.elf` artifact
2. Set `QEMU_SIL_ELF` env var to the downloaded path
3. Run `e_foc.qemu_sil_tests -- integration_tests/software_in_the_loop/features`
4. Parse + publish results

---

## Suggested commit sequence

```
feat(qemu): add SemihostingCan — hal::Can over semihosting stdin/stdout
feat(qemu): add SilQemuApplication — full motor stack wired for QEMU
refactor(qemu): split SilQemuMain into benchmark mode and interactive mode
feat(qemu): extend QemuSilSession/Fixture with CAN frame I/O
feat(qemu): add QEMU SIL step definitions (state machine, CAN, calibration)
feat(qemu): add QEMU SIL step definitions (speed, position, telemetry frame)
feat(qemu): build e_foc.qemu_sil_tests host executable
feat(ci): run e_foc.qemu_sil_tests in host_build_test_ubuntu job
```

---

## Estimated scope

| Phase | New files | Modified files | Size |
|---|---|---|---|
| 1 — SemihostingCan | 2 | 0 | ~80 lines |
| 2 — SilQemuApplication | 2 | 0 | ~150 lines |
| 3 — SilQemuMain refactor | 0 | 1 | ~20 lines |
| 4 — Session/Fixture extension | 0 | 2 | ~80 lines |
| 5 — QEMU step definitions | 5 | 1 | ~400 lines |
| 6 — Telemetry frame (optional) | 0 | 1 | ~30 lines |
| 7 — CMake + CI | 1 | 3 | ~60 lines |
| **Total** | **10** | **8** | **~820 lines** |
