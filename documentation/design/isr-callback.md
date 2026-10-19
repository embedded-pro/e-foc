---
title: "IsrCallback and HotSwapFunction: EMIL and the HALs"
type: design
status: draft
version: 0.1.0
component: async-safety
date: 2026-10-19
---

| Field     | Value                                              |
|-----------|----------------------------------------------------|
| Title     | IsrCallback and HotSwapFunction: EMIL and the HALs |
| Type      | design                                             |
| Status    | draft                                              |
| Version   | 0.1.0                                              |
| Component | async-safety                                       |
| Date      | 2026-10-19                                         |

---

## Responsibilities

**Is responsible for:**
- Stating the contract `foc::IsrCallback` relies on, and the cases where it does not hold
- Planning its move to EMIL as a generic type
- Listing the HAL callbacks that have the same defect, with the order to convert them
- Analysing priority inversion and the neighbouring hazards

**Is NOT responsible for:**
- The current use in e-foc, which is in `async-callback-lifetime.md`

---

## Component Details

### Part A — Why it exists

`infra::Function` assignment destroys the held callable, which sets the abort-on-execute
sentinel table, and then copy-constructs the new one (`Function.hpp`, `operator=`). An interrupt
that invokes the function between the two runs the sentinel and aborts. Any callback that is
replaced from the thread while its interrupt keeps firing has that window.

What the failure looks like depends on the call site. An unguarded call runs the abort sentinel.
A guarded call (`if (f) f();`, which is nearly every ISR-side call in the HALs) sees the
sentinel as empty and is skipped, so the event or the batch of bytes is silently dropped.
For a callable with a non-trivial destructor there is a third window: `Clear` and `= nullptr`
destroy the callable first and install the sentinel afterwards, so an interrupt landing between
the two calls a destroyed object. The vtable pointer being written last is an implementation
detail, with no barrier behind it.

### Part B — Moving it to EMIL

The type needs nothing from e-foc: it is `infra::Function` plus `std::atomic<uint8_t>`. It is
implemented as `infra::HotSwapFunction` on branch `feat/hot-swap-function` of the EMIL
repository (commit a9d9b01d, not pushed): `infra/util/HotSwapFunction.hpp`, void-returning only,
with the `ExtraSize` parameter of `infra::Function`, copy deleted, and twelve tests, among them
the key one, which calls the object from a copy-constructor hook in the middle of a `Set` and
must see a whole old function, and a negative control showing the same hook aborts a plain
`infra::Function` assignment.

Names considered: `HotSwapFunction` (chosen: says what it does, a function replaced while live,
and does not promise more than the contract gives), `InterruptSafeFunction` (the clearest to
a reader, but it promises safety against any interrupt, which the contract does not),
`DoubleBufferedFunction` (names the mechanism), `PublishedFunction` (names the atomic publish),
`IsrFunction` (short, but ties it to interrupts when any preempting invoker qualifies).

| Step | Work |
|------|------|
| 1 | Review and push the EMIL branch, open the pull request |
| 2 | Bump the EMIL pin in e-foc; make `foc::IsrCallback` an alias of `infra::HotSwapFunction`, then delete it once no caller names it |
| 3 | Update `async-callback-lifetime.md` to point at the EMIL header |

Whether to change `Function::operator=` instead was considered and rejected: the stored callable
is several words and no single store replaces it.

Open: a one-shot take-and-clear operation. The HAL survey in Part C found no member that needs it.

### Part C — The HAL drivers

Surveyed in `infra/hal/ti/hal_tiva/tiva` and `infra/hal/st/hal_st/stm32fxxx` by reading each
`infra::Function` and `AutoResetFunction` member against its interrupt handler, its assignment
sites and the order in which the interrupt is enabled. A first pass listed the CAN and GPIO
drivers as the likeliest to need the type; the reading showed that wrong, and this table
replaces it.

The local convention is not to hot-swap at all: assign before enabling the interrupt, often with
`really_assert(!f)` against overlap; mask the interrupt source around the assignment; hand off
to the dispatcher with an atomic flag or counter (`scheduled`, `pendingEdges`,
`pendingErrors`, `sending`); use `ATOMIC_SET_BIT` for register read-modify-write. The
dispatcher's own `Schedule` reserves a slot by compare-and-swap and publishes it afterwards, so
an interrupt scheduling work is safe.

| Verdict | Members |
|---------|---------|
| **Needs it** | ST `TimerWithInterruptStm::onIrq` and `LowPowerTimerWithInterruptStm::onIrq`: immediate by default, invoked unguarded, replaced by every `Start` with no assert and no mask, so a second `Start` on a running timer can abort |
| **Needs it, low likelihood** | The four UART `dataReceived` members (TI `Uart` and `UartWithDma`, ST `UartStm` and `UartStmDuplexDma`): replacing one while live drops the bytes the interrupt already drained. Masking the receive interrupt around the assignment is the cheaper alternative |
| **Hardening only** | TI `Adc::callback` and ST `AdcDmaMultiChannelStmBase::onDone` (a second `Measure` without `Stop` races; the failure is one dropped batch; with the type in the HAL, e-foc's arm-once workaround in `AdcPhaseCurrentMeasurement` could go), TI `AnalogComparator::onOutputChanged` (not used by e-foc) |
| **Not appropriate** | TI `Can` `onSendComplete` and `onReceive`, ST `CanStm`, ST `AdcTriggeredByTimerWithDma`, ST `AdcStm`: never invoked from an interrupt, only from the dispatcher, so the contest is thread against thread. TI and ST `Gpio` `handlers[]`: the check and the call are both inside the interrupt, which the thread cannot preempt, and TI asserts the slot empty and masks before assigning. One-shot `AutoResetFunction` members (`SpiMaster`, `I2c`, the DMA and flash drivers): armed before the operation, taken and cleared by the interrupt or the dispatcher, with overlap asserted away |
| **Already safe** | Members assigned in the constructor or before the interrupt is enabled and never replaced (TI and ST `WatchDog`, TI `Pwm`, `Dma`, ST DMA stream handlers), TI `Can::onError`, ST `ComparatorStm`, which orders disable, unregister and clear correctly |

Findings outside the type, found on the way:

- **TI `Can::onSendComplete` can be rebound before the queued completion runs.** The transmit
  interrupt clears `sending` and schedules a lambda that reads the member in the dispatcher. A
  `SendData` for the next frame in between passes the `!sending` assert and rebinds the member,
  so the first completion calls the second callback early and the first never runs. It stays
  hidden while the only sender is the completion callback itself. The fix is to capture the
  callback when the completion is queued, or to clear `sending` in the dispatcher lambda;
  `NotifySendFailedFromInterrupt` has the same shape.
- **ST `Gpio::EnableInterrupt` unmasks before it assigns** the handler and its interrupt type, so
  an edge in between is dropped or dispatched as the wrong type. TI orders it correctly: mask,
  assert empty, assign, clear the flag, unmask. The fix is the reorder.
- **TI and ST `Gpio` destroy the executing function** when `DisableInterrupt` is called from
  inside the handler: harmless for trivial captures and a use after destroy for the rest.
- **TI `Gpio` port interrupts scan all eight lines**, so an edge on one pin can run the handler
  while another is being enabled or disabled. It then skips an empty or half-assigned slot.

Recommended order: the two ST timers, the UARTs if the mask is judged too costly, then the two
ADC drivers if the workaround in e-foc is to go. The CAN and GPIO items are separate small
changes in the HAL repositories and do not need the type.

### Part D — Contract, priority inversion and the other hazards

**Priority inversion: none.** `Set`, `Clear` and the call are wait-free: no lock, no loop, no
waiting on another context. Nothing at a low priority can hold something a high-priority
context needs, which is what inversion requires. The cost is on the setter side only: one copy
of the callable.

**What can go wrong instead, in order of likelihood:**

1. **A setter that does not run at a lower priority than the invoker.** The slot argument needs
   the invoker to preempt the setter. If a `Set` or `Clear` ran at the same or a higher priority
   (a fault handler stopping the runner, say), the invoker could be mid-call in the slot being
   rewritten. In e-foc the phase-current interrupt and the PWM fault interrupt are both at the
   highest priority, so neither preempts the other; the fault path is safe only because it
   schedules the state change on the dispatcher (`Logic.cpp`), which makes the `Clear` a
   thread-level call. A new direct call from either interrupt breaks it. The type cannot detect
   this; a debug build could record the active IPSR on `Set` and assert it is the thread.
2. **A callback that outlives its captures.** `Clear` and `Set` return while the interrupt may
   still be running the old callable, because they do not wait. Whatever the old callable
   captured must outlive the next interrupt, not just the `Clear`. The runner and the ADC
   wrapper capture `this` of objects that live as long as the run, which holds today.
3. **More than one replace per call.** A callback that replaces itself twice in one call
   overwrites the slot it is running from.
4. **Two invokers.** Two interrupts of different priorities calling the same object break the
   single-reader assumption; a nested invoker could see the index change mid-call.
5. **More than one core.** The release and acquire pair orders the slot write before the index,
   but the slot being idle relies on the invoker being unable to run during the write, which
   multiple cores do not give. The type is single-core only and must say so.
6. **A lost clear.** If an interrupt clears the callback while the thread is inside `Set`, the
   thread's publish wins and the callback is armed again. That is the intended order for a
   one-shot being re-armed, and surprising otherwise.

**Priorities in the TI build today:** phase-current ADC and PWM fault at highest, CAN normal,
UART low, watchdog early warning lowest. The phase-current ADC is the only invoker of the
runner's callback and of the platform's, and both are set from the thread.

---

## Constraints & Limitations

- Single core only.
- One setter context, one invoker context per object.

## Open Questions

- Does EMIL want a type this specific, or the change to `Function::operator=` instead?
- Is an IPSR assertion acceptable in the debug build?
