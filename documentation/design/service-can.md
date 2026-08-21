---
title: "CAN Service Layer Design"
type: design
status: draft
version: 0.1.0
component: can-service
date: 2026-08-21
---

| Field     | Value                     |
|-----------|---------------------------|
| Title     | CAN Service Layer Design  |
| Type      | design                    |
| Status    | draft                     |
| Version   | 0.1.0                     |
| Component | can-service               |
| Date      | 2026-08-21                |

> **IMPORTANT — Implementation-blind document**: This document describes *behavior, structure, and
> responsibilities* WITHOUT referencing code. **No code blocks using programming languages (C++, C,
> Python, CMake, shell, etc.) are allowed.** Use Mermaid diagrams to express behavior instead.
> Prose descriptions of algorithms are encouraged; source-level details are not.
>
> **Diagrams**: All visuals must be either a Mermaid fenced code block or ASCII art inline
> in the document. External image references (`![alt](path)`) are **not allowed**.

---

## Responsibilities

**Is responsible for:**
- Defining the FOC motor CAN category (ID `0x02`) message format — command IDs, response IDs, scale factors, and error codes — in one canonical location.
- Implementing the server category: decoding incoming CAN frames into typed observer callbacks and encoding typed results into response frames.
- Implementing the client category: encoding typed setpoints into CAN frames and decoding response frames into typed callbacks.
- Bridging the server category to `ControlModeStateMachine`, translating motor control commands into state machine transitions.
- Providing a user-facing client API that composes the protocol client, transport, and category client into a single object for use by host-side tools and SIL tests.
- Applying tracing decorators at every layer boundary so all CAN frames and protocol lifecycle events are observable.
- Immediately returning `applicationError` for commands not yet implemented in this iteration.

**Is NOT responsible for:**
- The CAN protocol framing, sequence validation, or ACK dispatch — those belong to `can-lite` core.
- The FOC control algorithms, state machine transitions, or calibration — those belong to `core/foc/` and `core/state_machine/`.
- Motor physics or hardware-specific logic.

---

## Component Details

### Part A — FOC Motor Message Catalogue

All message type IDs, category ID, scale factors, and error codes are centralised in `FocMotorMessages.hpp`. No other component in `core/can/` or `core/state_machine/` re-defines these constants; all refer to this single source.

The category occupies slot `0x02` (the first application-reserved category ID). Commands occupy message types `0x00–0x7F`; responses occupy `0x80–0xFF`. The category error frame uses the `can-lite` reserved type `0xFE`.

Physical-to-wire conversions use fixed-point scale factors:

| Quantity  | Wire type | Scale factor | Example                          |
|-----------|-----------|--------------|----------------------------------|
| Current   | int16     | 10           | 1.5 A → 15 wire                  |
| Speed     | int16     | 1            | 300 rad/s → 300 wire             |
| Position  | int16     | 100          | 3.14 rad → 314 wire              |
| Voltage   | int16     | 10           | 24.0 V → 240 wire                |
| PID gain  | int16     | 1            | passed through unscaled          |
| Resistance| int16     | 1000         | 0.5 Ω → 500 wire                 |
| Inductance| int16     | 1000         | 1.0 mH → 1000 wire               |

### Part B — FocMotorCategoryServer

Inherits `CanCategoryServer` from `can-lite`. Registers one `CanMessageHandler` per command type; each handler decodes the payload using `CanPayloadReader`, skips the sequence byte (required for server categories), converts wire values to physical units, and calls the corresponding observer method.

The observer interface provides callbacks for:
- `OnStart`, `OnStop`, `OnClearFault`, `OnEmergencyStop` — lifecycle commands with a `CanAckStatus` result callback.
- `OnSelectControlMode` — takes a `FocMotorMode` and a result callback.
- `OnSetTorqueSetpoint`, `OnSetSpeedSetpoint`, `OnSetPositionSetpoint` — take a typed unit quantity and a completion callback.
- All other commands (`OnSetPidCurrent`, `OnSetPidSpeed`, `OnSetPidPosition`, `OnIdentifyElectrical`, `OnIdentifyMechanical`, `OnRequestTelemetry`, `OnSetEncoderResolution`, `OnConfigureTelemetryRate`) — the default bridge implementation returns `applicationError` immediately.

### Part C — FocMotorCategoryClient

Inherits `CanCategoryClient` from `can-lite`. Provides typed `Send*` methods that encode physical units into wire format via `CanPayloadWriter` and call `SendCommand(nodeId, messageType, payload)`.

Registers response handlers for the server-to-client response message types. Response handlers decode the payload and invoke the corresponding observer callbacks.

### Part D — FocMotorCanBridge

Implements the `FocMotorCategoryServerObserver` interface. Holds references to `FocMotorCategoryServer` and `ControlModeStateMachine`. Translates each observer callback into the appropriate state machine call:

- `OnStart` → `CmdEnable()`; success calls the ack callback, rejection sends `invalidState`.
- `OnStop` → `CmdDisable()`.
- `OnClearFault` → `CmdClearFault()`.
- `OnEmergencyStop` → `CmdEmergencyStop()`.
- `OnSelectControlMode` → `Select(mode, onDone)`; the result callback sends the mode response or a category error.
- `OnSetTorqueSetpoint`, `OnSetSpeedSetpoint`, `OnSetPositionSetpoint` → validate mode and range, then delegate to `TrySet*` on the state machine.
- All stub commands → `SendCategoryError(commandId, applicationError)`.

The bridge validates that the correct control mode is active before accepting a setpoint command. An out-of-range setpoint results in `invalidPayload`. A mode mismatch results in `categoryError/modeMismatch`.

### Part E — FocMotorCanClient

A self-contained client object for use in host tools and SIL tests. Internally composes `CanFrameTransport`, `CanProtocolClient`, and `FocMotorCategoryClient` as value members (no heap). Constructed with a `hal::Can&` reference and a target node ID.

Provides the same `Send*` API as `FocMotorCategoryClient` but hides the transport and protocol client, so callers only need a CAN bus reference and a node ID.

### Part F — Tracing Decorators

All tracing is applied via `can-lite`'s decorator classes:

| Decorator                        | Wraps                   | Traces                                         |
|----------------------------------|-------------------------|------------------------------------------------|
| `TracingCan`                     | `hal::Can`              | Every raw CAN frame sent and received          |
| `TracingCanProtocolServerObserver` | `CanProtocolServer`   | Server online/offline events                   |
| `TracingCanProtocolClientObserver` | `CanProtocolClient`   | Server online/offline, ACK timeouts per node   |

These are composed in the instantiation layer (`Logic.hpp`) so the production code paths gain full observability without any tracing logic in the category or bridge classes themselves.

---

## Interfaces

### Provided

| Interface                      | Purpose                                                                           | Contract                                                                         |
|--------------------------------|-----------------------------------------------------------------------------------|----------------------------------------------------------------------------------|
| FocMotorCategoryServerObserver | Observer interface for the server category                                        | Callbacks must not block or allocate; called from the event dispatcher context   |
| FocMotorCategoryClientObserver | Observer interface for the client category                                        | Callbacks must not block or allocate                                              |
| FocMotorCanClient public API   | User-facing send methods (`Start`, `Stop`, `SetTorque`, `SetSpeed`, `SetPosition`) | Returns `bool` indicating whether the frame was queued; ACK is delivered async   |

### Required

| Interface                 | Purpose                                          | Contract                                                                  |
|---------------------------|--------------------------------------------------|---------------------------------------------------------------------------|
| `CanFrameTransport`       | Sends and receives raw CAN frames                | Provided by `can-lite`; must be constructed before the category objects   |
| `ControlModeStateMachine` | Accepts motor control commands                   | All methods are synchronous or deliver results via `infra::Function`      |
| `hal::Can`                | Raw CAN bus access                               | Required by `CanProtocolServer` and `CanProtocolClient`                   |
| `services::Tracer`        | Diagnostic output                                | Must outlive the tracing decorator objects                                |

---

## Data Model

| Entity                  | Field          | Type / Unit | Range                 | Notes                            |
|-------------------------|----------------|-------------|------------------------|----------------------------------|
| FocMotorMode            | mode           | enum uint8  | torque=0, speed=1, position=2 | Transmitted as single byte |
| Torque setpoint         | iq             | int16 wire  | INT16_MIN – INT16_MAX  | Physical = wire / 10 [A]        |
| Speed setpoint          | speed          | int16 wire  | INT16_MIN – INT16_MAX  | Physical = wire / 1 [rad/s]     |
| Position setpoint       | position       | int16 wire  | INT16_MIN – INT16_MAX  | Physical = wire / 100 [rad]     |
| FocMotorCategoryError   | code           | enum uint8  | 0–5                    | See FocMotorMessages.hpp        |

---

## Sequence Diagrams

### Start Command (server path)

```mermaid
sequenceDiagram
    participant CAN as CAN Bus
    participant SRV as FocMotorCategoryServer
    participant BRG as FocMotorCanBridge
    participant SM as ControlModeStateMachine
    participant ACK as CanProtocolServer (ACK)

    CAN->>SRV: HandleMessage(focStartId, payload)
    SRV->>BRG: OnStart(ackCallback)
    BRG->>SM: ActiveStateMachine().CmdEnable()
    SM-->>BRG: CommandResult::ok
    BRG->>ACK: ackCallback(CanAckStatus::success)
    ACK->>CAN: SendCommandAck(category=0x02, type=0x01, success)
```

### SelectControlMode (server path)

```mermaid
sequenceDiagram
    participant CAN as CAN Bus
    participant SRV as FocMotorCategoryServer
    participant BRG as FocMotorCanBridge
    participant SM as ControlModeStateMachine
    participant NVM as NonVolatileMemory

    CAN->>SRV: HandleMessage(focSelectControlModeId, payload)
    SRV->>BRG: OnSelectControlMode(mode, responseCallback)
    BRG->>SM: Select(mode, onDone)
    SM->>NVM: SaveConfig(...)
    NVM-->>SM: Ok
    SM-->>BRG: SelectResult::ok
    BRG->>SRV: SendResponse(focSelectControlModeResponseId, activeMode)
    BRG->>ACK: responseCallback(CanAckStatus::success)
```

### Stub Command (applicationError path)

```mermaid
sequenceDiagram
    participant CAN as CAN Bus
    participant SRV as FocMotorCategoryServer
    participant BRG as FocMotorCanBridge

    CAN->>SRV: HandleMessage(focSetPidCurrentId, payload)
    SRV->>BRG: OnSetPidCurrent(gains, callback)
    BRG->>SRV: SendCategoryError(focSetPidCurrentId, applicationError)
    SRV->>CAN: categoryError frame
```

---

## Block Diagram

```mermaid
graph LR
    subgraph "Server Path (embedded target)"
        HAL_CAN_S["hal::Can"] --> TC["TracingCan"]
        TC --> CPS["CanProtocolServer"]
        CPS --> CPSO["TracingCanProtocolServerObserver"]
        CPS --> MCS["FocMotorCategoryServer"]
        MCS --> BRG["FocMotorCanBridge"]
        BRG --> SM["ControlModeStateMachine"]
    end

    subgraph "Client Path (host tools)"
        HAL_CAN_C["hal::Can"] --> CPC["CanProtocolClient"]
        CPC --> CPCO["TracingCanProtocolClientObserver"]
        CPC --> MCC["FocMotorCategoryClient"]
        MCC --> FMC["FocMotorCanClient (user API)"]
    end
```

---

## Constraints & Limitations

| Constraint                   | Value / Description                                                                              |
|------------------------------|--------------------------------------------------------------------------------------------------|
| Stub command policy          | Commands not yet implemented return `applicationError` via `SendCategoryError` immediately       |
| No heap                      | All objects are value members or statically allocated; `infra::Function` for callbacks           |
| One observer per server      | `CanCategoryServer` uses `infra::Subject<Observer>` — only one bridge may attach at a time       |
| Setpoint range               | Torque: bounded by inverter `MaxCurrentSupported`; speed: 1000 rad/s; position: ±2π rad         |
| Sequence byte                | Server handlers always skip the first byte (sequence number) before reading payload fields       |

---

## Open Questions

| # | Question                                                          | Options                                               | Status |
|---|-------------------------------------------------------------------|-------------------------------------------------------|--------|
| 1 | Implement telemetry push (BroadcastFaultStatus, periodic status)  | Periodic timer vs on-demand vs fault-triggered        | open   |
| 2 | PID gain commands                                                 | Accept and persist vs remain applicationError         | open   |
| 3 | Mechanical identification via CAN                                 | Delegate to existing service vs stub                  | open   |
