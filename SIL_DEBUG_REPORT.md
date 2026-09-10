# SIL Test Debug Report
**Last updated:** 2026-09-10  
**Branch:** refactor/merge-sil-hil-integration-tests

---

## Current test results

| Feature | Scenario | Status |
|---|---|---|
| calibration.feature | Torque calibration | ✔ PASS |
| calibration.feature | Speed calibration | ✔ PASS |
| calibration.feature | Position calibration | ✔ PASS |
| motor_control.feature | Torque mode | ✔ PASS |
| motor_control.feature | Speed mode | ✔ PASS (after root cause 3 fix) |
| motor_control.feature | Position mode | ✔ PASS |

---

## Root cause 1 — FIXED: Stale commandAck consumed by `SendCanCommand`

### What was happening

The firmware responds to every command — including telemetry-request frames
(`focRequestTelemetryId`, type=`0x08`) — with **two** CAN frames:

1. The payload response (e.g. telemetry `0xc289001`)
2. A generic commandAck (`0x8002001`)

`WaitForMotorState` calls `WaitForCanFrame(telemetryId=0xc289001)` in a loop. Each
iteration reads the telemetry response and returns, but the commandAck
`0x8002001 [cat=2, type=0x08, status=success]` was left **unread in the pipe**.

The very next `SendCanCommand(calibrate)` called `WaitForCanFrame(ackId=0x8002001, 60s)`.
Because the stale telemetry-request ACK matched the ACK CAN ID, `SendCanCommand`
read it and saw `ackPayload[2]=0=success` — returning `true` even though calibration
had failed with `categoryError`. The real calibrate ACK arrived later and was
silently dropped by the next `WaitForMotorState`.

### Evidence from logs (`SIL_VERBOSE=1`)

```
# WaitForMotorState (SelectControlMode) reads telemetry, leaves commandAck in pipe:
[QEMU->host] CAN_TX 0c289001 000000000000    ← telemetry consumed by WaitForMotorState
# 0x8002001 [02 08 00 00] left unread ↑

# SendCanCommand(calibrate) reads the stale commandAck:
[host->QEMU] CAN_RX 04206001 02              ← calibrate command sent
# WaitForCanFrame(0x8002001) immediately reads stale 8002001 02080000
# ackPayload=[0x02,0x08,0x00,0x00] → ackPayload[2]=0=success → returns TRUE (WRONG)

# Actual calibrate response arrives too late — dropped:
[QEMU->host] CAN_TX 082fe001 0603            ← categoryError notification
[CAN drop] parsed=082fe001 expected=0c289001  ← dropped by WaitForMotorState
[QEMU->host] CAN_TX 08002001 02060700        ← real calibrate ACK (status=categoryError=0x07)
[CAN drop] parsed=08002001 expected=0c289001  ← also dropped
```

### Fix applied

**File:** `integration_tests/support/Fixture.cpp`  
`SendCanCommand` now loops over `WaitForCanFrame` until it receives a commandAck
whose `ackPayload[1]` (messageType field) matches the command's own messageType.
Stale ACKs for unrelated commands (type=`0x08` telemetry) are silently skipped:

```cpp
while (true)
{
    if (!WaitForCanFrame(ackId, ackPayload, remaining, elapsed))
        return false;
    if (ackPayload.size() < 3 || ackPayload[1] != messageType)
        continue;  // stale ACK for a different command — skip
    ++nextSequence;
    return ackPayload[2] == static_cast<uint8_t>(services::CanAckStatus::success);
}
```

---

## Root cause 2 — FIXED: `FocTimerIsr` feeds stale phase currents

### What was happening

In `PlatformFactoryImpl.cpp`, the `FocTimerIsr` fires at 20 kHz (simulated).
`lastCurrents` was populated **once** when `PhaseCurrentsReady` was called during
app initialisation. After that, `model.StepForTest` advanced the simulation on
every tick but `lastCurrents` was **never updated** — the FOC algorithm received
the same stale initial currents at every 20 kHz tick.

The R/L estimation algorithm injected a sinusoidal current pattern and expected
to see a coherent response. With constant currents, fit quality stayed below 0.5
and the algorithm returned failure → `calibrationFailed` → `categoryError`.

### Evidence from logs (`SIL_VERBOSE=1`, firmware tracer)

```
# focIdentifyElectricalId received only ~6ms after SelectControlMode:
[QEMU->host] CAN_TX 082fe001 0603   ← categoryError notification (calibrationFailed)
[QEMU->host] CAN_TX 08002001 02060700  ← ACK status=0x07=categoryError

# Firmware tracer showed no [SM] Calibrating output before the failure
# ~6ms turnaround = algorithm aborted on first sample (zero-variance currents)
```

### Fix applied

**File:** `targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.cpp`

`FocTimerIsr` now calls `model.PhaseCurrentsReady` on every tick to refresh
`lastCurrents` from the updated model state. `PhaseCurrentsReady` no longer
registers the callback (the ISR owns it):

```cpp
void PlatformFactoryImpl::FocTimerIsr()
{
    model.StepForTest(lastDutyPhases);
    model.PhaseCurrentsReady(baseFrequency, [this](foc::PhaseCurrents currents)
        { lastCurrents = currents; });
    if (!onPhaseCurrentsReady) return;
    // ... guard and dispatch ...
    onPhaseCurrentsReady(lastCurrents);
}
```

---

## Root cause 3 — FIXED: `WaitForMotorState` sequence divergence causes `sequenceError` on next command

### What was happening

`CanProtocolServer` validates monotonically-increasing sequence numbers on **every**
incoming CAN frame, including telemetry-request polls. It also rate-limits at
**500 msg/s** (`maxMessagesPerSecond` in `CanProtocolServer::Config`). When the
rate limit is hit, the frame is **dropped before sequence validation** — the
server's `lastSequenceNumber` does not advance, but the fixture's `nextSequence`
already incremented.

`WaitForMotorState` sends one request per loop iteration without any inter-poll
floor. In the "When the motor is enabled" step, `WaitForMotorState(idle, 500ms)`
polls for the full 500 ms because the motor is running (never returns idle). On a
fast CI runner where QEMU executes near real-time (1 ms canPollTimer = ~1 ms wall
time), this produces ≈500–1000 polls/s, saturating the rate limiter. Each dropped
poll widens the gap between `nextSequence` and `lastSequenceNumber`.

When the **speed setpoint** command then arrives with `seq = lastSeq + K + 1`
(K = number of dropped polls), the server sends a `sequenceError` ACK:

```
ackPayload = [cat=2, type=0x10, status=0x04=sequenceError, expected=X]
```

Unlike stale telemetry ACKs (type=`0x08`), this ACK has `ackPayload[1] = 0x10 =
focSetSpeedSetpointId` — it **passes** the `SendCanCommand` messageType filter and
causes the function to return `false`.

Position mode was unaffected in most CI runs because its scenario happened to land
in a fresher 1-second rate-limit window, leaving the gap below the failure
threshold.

### Evidence from logs

To reproduce, run with `SIL_VERBOSE=1` and watch for the sequence-error ACK
immediately after the speed setpoint is sent:

```
# Speed setpoint sent:
[host->QEMU] CAN_RX 04210001 <seq> 00 0a   ← focSetSpeedSetpointId=0x10, seq=N

# Server rejected it — sequence diverged during WaitForMotorState polling:
[QEMU->host] CAN_TX 08002001 02100400       ← cat=2, type=0x10, status=0x04=sequenceError, expected=M
# ackPayload[1]=0x10 matches messageType → NOT filtered → SendCanCommand returns false
```

The divergence builds during the 500 ms idle-check polling that precedes the
running-state poll in the motor-enable step. Setting `SIL_VERBOSE=1` and grepping
for `[Fixture] WaitForMotorState` shows hundreds of back-to-back polls with elapsed
times < 2 ms each — far above the safe polling rate.

### Fix applied

**File:** `integration_tests/support/Fixture.cpp`

`WaitForMotorState` now enforces a **20 ms minimum poll interval** (≤50 polls/s),
keeping the polling rate well below the server's 500 msg/s limit on any CI runner:

```cpp
static constexpr auto kMinPollInterval = std::chrono::milliseconds{ 20 };
...
const auto pollStart = std::chrono::steady_clock::now();
// ... send request, wait for response ...
const auto pollElapsed = std::chrono::steady_clock::now() - pollStart;
if (pollElapsed < kMinPollInterval)
{
    const auto sleepMs = duration_cast<milliseconds>(kMinPollInterval - pollElapsed);
    if (sleepMs > 0ms) usleep(sleepMs.count() * 1000);
}
```

At ≤50 polls/s the worst-case gap after a 500 ms wait is 25 missed sequences (if
every other poll were rate-limited, which cannot happen at 50 polls/s vs 500/s
limit).

---

## Available logs and how to collect them

### QEMU wire trace (`SIL_VERBOSE=1`)

Set `SIL_VERBOSE=1` before running to enable full bidirectional CAN logging via
`QemuSilSession` stderr:

| Prefix | Meaning |
|---|---|
| `[host->QEMU] CAN_RX <id> <data>` | CAN frame sent from test to firmware |
| `[QEMU->host] CAN_TX <id> <data>` | CAN frame received from firmware |
| `[CAN drop] parsed=X expected=Y` | Frame received but CAN ID didn't match what was waited for |
| `[QEMU] firmware crash: <line>` | `ABORT` sentinel detected in firmware output |
| `[Fixture] WaitForMotorState: got state=N expected=M elapsed=Xms` | Per-poll state check result |

```bash
SIL_VERBOSE=1 timeout 120 \
  build/host/integration_tests/main/Debug/e_foc.integration_tests \
  --mode sil integration_tests/features/motor_control.feature \
  -t "@sil" --format pretty 2>&1 | tee /tmp/sil.log
```

### Firmware tracer output

The firmware writes state-machine traces to semihosting stdout (the same pipe as
CAN_TX frames). These appear in the QEMU output captured by `outPipeFd`. With
`SIL_VERBOSE=1`, every line including tracer lines is shown via `[QEMU->host]`:

| Line | Meaning |
|---|---|
| `READY` | QEMU firmware boot complete; test can start sending CAN |
| `[SM] Entering Calibrating` | State machine entered Calibrating |
| `[SM] Entering Enabled` | State machine entered Enabled (motor running) |
| `[SM] Entering Ready` | State machine entered Ready (calibrated, stopped) |
| `[SM] Entering Fault` | State machine entered Fault |
| `[SM] Emergency stop` | `CmdEmergencyStop()` called (e.g. liveness watchdog) |
| `[CAN] Client lost while enabled, stopping drive` | Liveness watchdog fired |
| `[EST] Mech: J=... B=...` | Online mechanical estimator values |
| `[EST] Elec: R=... L=...` | Online electrical estimator values |

### GDB session for single-scenario debugging

```bash
# Terminal 1 — run the failing scenario with GDB server exposed:
SIL_GDB=1 build/host/integration_tests/main/Debug/e_foc.integration_tests \
  --mode sil integration_tests/features/motor_control.feature \
  -t "@sil and @REQ-SM-006" --format pretty &

# Terminal 2 — connect GDB:
arm-none-eabi-gdb \
  build/qemu-foc-sensored/targets/sync_foc_sensored/main/RelWithDebInfo/e_foc.sync_foc_sensored.main.elf
(gdb) target remote :1234

# Useful breakpoints and logging commands:
(gdb) break application::PlatformFactoryImpl::FocTimerIsr
(gdb) commands
> silent
> printf "ISR: a=%f b=%f\n", lastCurrents.a.value, lastCurrents.b.value
> continue
> end

(gdb) break can::FocMotorCanBridge::OnSetSpeedSetpoint
(gdb) commands
> silent
> printf "speed setpoint: value=%f\n", value._value
> continue
> end

(gdb) break services::CanProtocolServer::ValidateSequence
(gdb) commands
> silent
> printf "ValidateSeq: got=%d expected=%d init=%d\n", sequenceNumber, \
         (unsigned char)(lastSequenceNumber+1), sequenceInitialized
> continue
> end

(gdb) continue
```

### Decoding CAN IDs

All IDs are 29-bit extended. Layout: `[28:24]=priority [23:20]=category [19:12]=type [11:0]=nodeId`.

```
Priority:  heartbeat=0x10 telemetry=0x0C response=0x08 command=0x04 emergency=0x00
Category:  system=0x0 foc_motor=0x2
Node:      server=0x001

Common CAN IDs on the wire:
  0x04208001  focRequestTelemetryId   (command,  cat=2, type=0x08)
  0x0C289001  focTelemetryStatusResponse (telemetry, cat=2, type=0x89)
  0x08002001  commandAck              (response, cat=0, type=0x02)
  0x10001001  heartbeat               (heartbeat, cat=0, type=0x01)
  0x04206001  focIdentifyElectricalId (command,  cat=2, type=0x06)
  0x04201001  focStartId              (command,  cat=2, type=0x01)
  0x04210001  focSetSpeedSetpointId   (command,  cat=2, type=0x10)
  0x04211001  focSetPositionSetpointId (command, cat=2, type=0x11)
```

ACK payload format (always 4 bytes):
```
[0] category      e.g. 0x02 = foc_motor
[1] commandType   e.g. 0x10 = focSetSpeedSetpointId
[2] status        0x00=success 0x02=invalidPayload 0x03=invalidState
                  0x04=sequenceError 0x07=categoryError
[3] expectedSeq   server's expected next sequence (non-zero on sequenceError)
```

---

## How to reproduce the sequence-divergence bug (before the fix)

Check out the commit before the fix and run with a fast machine or inject an
artificial delay in the test to simulate the rate-limit being hit:

```bash
# Simulate high polling rate by temporarily removing WaitForCanFrame wait:
# Set kMinPollInterval = 0ms (or revert the fix) then run:
SIL_VERBOSE=1 timeout 60 \
  build/host/integration_tests/main/Debug/e_foc.integration_tests \
  --mode sil integration_tests/features/motor_control.feature \
  -t "@sil and @REQ-SM-006" --format pretty 2>&1 | \
  grep -E "host->QEMU|QEMU->host.*0x10|sequenceError|speed setpoint"
```

Look for `[QEMU->host] CAN_TX 08002001 02100400` — the `04` in byte 3 is
`sequenceError`, and `02100400` means `cat=2, type=0x10, status=0x04, expected=0`.

---

## Secondary issues (non-blocking)

### `EEPROM_FLUSH fopen=0` — semihosting `fopen` failure

`SemihostingEeprom::FlushToFile` logs `fopen=0` on every write. The code falls
back to raw semihosting `SYS_OPEN/SYS_WRITE` which succeeds. In-memory EEPROM
state is consistent within a single QEMU run. Cross-run persistence (NVM surviving
QEMU restart) is not tested and not required by current scenarios — each scenario
starts with a fresh QEMU and `/tmp/eeprom.bin` is deleted in `QemuSilSession::Start`.

---

## Files changed across all sessions

| File | Change |
|---|---|
| `integration_tests/support/Fixture.cpp` | Fix 1: `SendCanCommand` stale-ACK loop; Fix 3: 20 ms poll floor in `WaitForMotorState` |
| `integration_tests/support/interactor/qemu/QemuSilSession.cpp` | ABORT detection; `SIL_VERBOSE` wire logging; `SIL_GDB` mode; 4096-byte read buffer |
| `integration_tests/support/interactor/qemu/QemuInteractor.cpp` | `GTEST_FAIL()` on QEMU start failure; clear lines in `BeforeScenario` |
| `integration_tests/support/interactor/qemu/CMakeLists.txt` | Added `GTest::gtest` link |
| `integration_tests/steps/MotorControlSteps.cpp` | Added motor-control step definitions for all three modes |
| `integration_tests/steps/StateMachineSteps.cpp` | Added state-machine step definitions |
| `integration_tests/features/motor_control.feature` | New end-to-end motor control scenarios |
| `integration_tests/features/calibration.feature` | New end-to-end calibration scenarios |
| `targets/platform_implementations/qemu/implementation/PlatformFactoryImpl.cpp` | Fix 2: refresh `lastCurrents` on every `FocTimerIsr` tick |
| `targets/platform_implementations/qemu/implementation/SemihostingEeprom.cpp/hpp` | File-backed EEPROM via semihosting |
| `targets/platform_implementations/qemu/implementation/SemihostingCan.cpp` | CAN RX/TX over CMSDK UART |
| `motor_parameters/Jk42bls01X038ed.hpp` | `maxSupportedCurrent=200A` for QEMU (no physical OC protection) |
| `core/state_machine/ControlModeStateMachine.cpp` | `AcceptExternalCalibration` wired through for CAN-triggered path |
| `core/can/FocMotorCanBridge.cpp` | `AcceptExternalCalibration` bridge handler |
