---
title: "Async Callback Lifetime Safety"
type: design
status: accepted
version: 1.0.0
component: async-safety
date: 2026-09-14
---

| Field     | Value                          |
|-----------|--------------------------------|
| Title     | Async Callback Lifetime Safety |
| Type      | design                         |
| Status    | accepted                       |
| Version   | 1.0.0                          |
| Component | async-safety                   |
| Date      | 2026-09-14                     |

---

## Responsibilities

**Is responsible for:**
- Defining the project-wide pattern for scheduling event-dispatcher closures on objects whose lifetime may end before the closure fires
- Specifying when and how `HasPendingAsyncWork()` must be extended to reflect all active async operations
- Documenting the invariant that the `AccessedBySharedPtr` control block relies on, and how the codebase maintains it

**Is NOT responsible for:**
- Real-time ISR paths — this pattern applies only to work dispatched through the main event loop
- General shared-ownership semantics unrelated to async scheduling
- Heap-allocated object management (this pattern is exclusively for stack- and statically-allocated objects)

---

## Background: The Use-After-Free Risk

Services and notifiers in e-foc schedule deferred work on the global event dispatcher. A typical flow:

```mermaid
sequenceDiagram
    participant Service
    participant EventDispatcher
    participant FSM as ControlModeStateMachine

    Service->>EventDispatcher: Schedule(closure capturing this)
    Note right of EventDispatcher: closure is queued
    FSM->>Service: Abort()
    Note right of Service: state cleared
    FSM->>FSM: mode switch → destroys Service
    EventDispatcher->>Service: execute closure
    Note right of Service: ⚠ this is now destroyed
```

Reading any member through `this` after the object is destroyed is undefined behaviour. The same risk applies when the FSM variant is replaced while a closure referencing a sub-object is still in the queue.

---

## Solution: Two Layers

The codebase defends against this with two complementary mechanisms that operate at different scopes.

### Layer 1 — Dispatch Safety: WeakPtr-Bound Closures

Every service that schedules event-dispatcher closures inherits `EnableSharedFromThis<T>` and is constructed inside a `WithSharedAccess<T>` wrapper. `WithSharedAccess<T>` bundles the object with its `AccessedBySharedPtr` control block. Any `WeakPtr<T>` derived via `WeakFromThis()` expires exactly when the wrapper is destroyed.

Closures are scheduled using the WeakPtr overload of `EventDispatcherWithWeakPtr`. When the dispatcher dequeues a closure, it attempts to lock the WeakPtr. If the object was already destroyed, the attempt returns null and the closure is silently discarded — no member is read, no undefined behaviour occurs.

```mermaid
sequenceDiagram
    participant Service as Service (WithSharedAccess)
    participant Dispatcher as EventDispatcherWithWeakPtr
    participant FSM

    Service->>Dispatcher: Schedule(action, WeakFromThis())
    Note right of Dispatcher: stores WeakPtr + action
    FSM->>Service: destroys WithSharedAccess
    Note right of Service: WeakPtr expires
    Dispatcher->>Dispatcher: lock WeakPtr → null
    Note right of Dispatcher: action discarded ✓
```

**Invariant enforced by `AccessedBySharedPtr`:** The control block asserts that no WeakPtrs remain outstanding at the moment the wrapper is destroyed. In production, this invariant is maintained because the event dispatcher processes closures in FIFO order: a closure scheduled before the mode-switch NVM callback fires before the mode-switch takes effect, releasing the WeakPtr in time. In tests, fixtures must drain the dispatcher (via `ExecuteAllActions()`) in `TearDown()` before any `WithSharedAccess`-wrapped member is destroyed.

### Layer 2 — Business-Logic Guard: `HasPendingAsyncWork()`

The WeakPtr mechanism ensures memory safety regardless of timing. The second layer ensures correct application behaviour: no mode switch should proceed while an identification service is actively driving hardware.

`HasPendingAsyncWork()` aggregates the running state of all async services:

```mermaid
graph TD
    A[HasPendingAsyncWork] --> B{pending NVM command?}
    A --> C{boot NVM check in flight?}
    A --> D{Calibrating state?}
    A --> E{electricalIdent.IsRunning?}
    A --> F{mechIdent.IsRunning?}
    B -- yes --> G[return true]
    C -- yes --> G
    D -- yes --> G
    E -- yes --> G
    F -- yes --> G
```

`ControlModeStateMachine::Select()` checks `HasPendingAsyncWork()` before initiating a mode switch. If it returns `true`, the select returns `busy` and the caller retries. If it returns `false`, the mode switch proceeds.

Each identification service exposes `IsRunning()` on its interface. This method reports whether the service is actively running an identification sweep — not merely whether a dispatcher slot is pending. The `HasPendingAsyncWork()` check at mode-switch time and the WeakPtr discard at dispatch time together cover all failure scenarios.

---

## Component Details

### `WithSharedAccess<T>` Wrapper

`WithSharedAccess<T>` is a non-copyable value type that contains:
- The `AccessedBySharedPtr` control block (anchor)
- The `T` object itself
- A `SharedPtr<T>` that keeps the anchor alive for the wrapper's lifetime

Member declaration order is load-bearing: `sharedPtr` is destroyed first (drops the shared count), then `T` (which destroys `EnableSharedFromThis::weakPtr`, making the control block unreferenced), then `accessedBy` (which asserts the control block is unreferenced at destruction time). Reversing this order would trigger the assertion.

`T` must inherit `EnableSharedFromThis<T>`. `WithSharedAccess<T>` construction arguments are forwarded to `T`'s constructor.

### `HasPendingAsyncWork()` Contract

Every class in the `FocStateMachine` hierarchy that adds async operations must also extend the return value of `HasPendingAsyncWork()` to cover those operations.

The base implementation (`FocStateMachineCommon`) covers:
- Pending NVM commands (`HasPendingCommand()`)
- Boot-time NVM validity check (`bootCheckInFlight`)
- Active calibration state (`Calibrating` variant)
- Direct CAN electrical identification (`electricalIdent.IsRunning()`)

`OuterLoopStateMachine` overrides to additionally cover:
- Direct CAN mechanical identification (`mechIdent.IsRunning()`)

Any future service that exposes identification or async operations reachable outside the `Calibrating` state must add a corresponding `IsRunning()` check here.

### `IsRunning()` Interface

Each identification service interface exposes `IsRunning() const` returning `true` while an estimation is in progress and `false` otherwise. The implementation tracks whether the internal estimation state is populated. Calling `Abort()` resets the state and causes `IsRunning()` to return `false` immediately.

---

## Sequence Diagrams

### Normal Mode Switch (No Pending Work)

```mermaid
sequenceDiagram
    participant CAN as CAN Bridge
    participant CSM as ControlModeStateMachine
    participant FSM as ActiveFocStateMachine

    CAN->>CSM: Select(newMode)
    CSM->>FSM: HasPendingAsyncWork()
    FSM-->>CSM: false
    CSM->>CSM: SaveConfig to NVM
    CSM->>FSM: CmdEmergencyStop()
    CSM->>CSM: emplace<NewMode>()
    Note right of CSM: WithSharedAccess anchors destroyed
    Note right of CSM: WeakPtrs for any pending closures expire
```

### Mode Switch Blocked (Identification Running)

```mermaid
sequenceDiagram
    participant CAN as CAN Bridge
    participant CSM as ControlModeStateMachine
    participant FSM as ActiveFocStateMachine
    participant Ident as IdentificationService

    Ident->>Ident: EstimateFrictionAndInertia() started
    CAN->>CSM: Select(newMode)
    CSM->>FSM: HasPendingAsyncWork()
    FSM->>Ident: IsRunning()
    Ident-->>FSM: true
    FSM-->>CSM: true
    CSM-->>CAN: busy
```

### Emergency Stop During Pending Dispatch

```mermaid
sequenceDiagram
    participant Ident as MechanicalIdent (WithSharedAccess)
    participant ED as EventDispatcherWithWeakPtr
    participant FSM

    Ident->>ED: Schedule(convergenceAction, WeakFromThis())
    FSM->>Ident: Abort() — clears rls
    FSM->>FSM: CmdEmergencyStop()
    FSM->>FSM: mode switch → destroys WithSharedAccess
    Note right of Ident: WeakPtr expires
    ED->>ED: lock WeakPtr → null
    Note right of ED: action discarded safely ✓
```

---

## Constraints & Limitations

| Constraint | Description |
|------------|-------------|
| `AccessedBySharedPtr` invariant | All `WeakPtr` instances must be released before the `WithSharedAccess` wrapper is destroyed. In tests, this requires `ExecuteAllActions()` in `TearDown()` for every fixture that holds a `WithSharedAccess`-wrapped member. |
| FIFO ordering required | The safety argument for production code depends on the event dispatcher processing closures in FIFO order. Any scheduler change that breaks FIFO ordering must be audited against this pattern. |
| `T` must inherit `EnableSharedFromThis<T>` | `WithSharedAccess<T>` provides no safety benefit if `T` does not inherit `EnableSharedFromThis<T>`, because `WeakFromThis()` would return a null pointer and all closures would be discarded regardless of object lifetime. |
| Static objects exempt | Objects with application-static lifetime (e.g., the top-level `Logic` composite) do not require this pattern because they can never be destroyed while closures are pending. |
