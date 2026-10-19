---
title: "IsrCallback: Moving It to EMIL and Using It in the HALs"
type: design
status: draft
version: 0.1.0
component: async-safety
date: 2026-10-19
---

| Field     | Value                                              |
|-----------|----------------------------------------------------|
| Title     | IsrCallback: Moving It to EMIL and Using It in the HALs |
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

### Part B — Moving it to EMIL

The type needs nothing from e-foc: it is `infra::Function` plus `std::atomic<uint8_t>`.

| Step | Work |
|------|------|
| 1 | Open a proposal on `embedded-pro/embedded-infra-lib` with the failure above, the contract of Part D and the two alternatives (below) |
| 2 | Add `infra/util/IsrFunction.hpp` (working name; `AtomicFunction` is wrong, the assignment is not atomic) with the same interface, and a test file beside `TestFunction.cpp` |
| 3 | Tests: set, replace, clear, call with arguments, an invoker that fires from inside a `Set` (a hook that calls the function from the copy constructor stands in for the interrupt), a callback that replaces itself once, and a compile test on the smallest core EMIL builds for |
| 4 | Bump the EMIL pin in e-foc; make `foc::IsrCallback` an alias of the EMIL type, then delete it once no caller names it |
| 5 | Update `async-callback-lifetime.md` to point at the EMIL header |

Alternatives for the proposal: change `Function::operator=` itself, which cannot close the window
either, because the stored callable is several words and no single store replaces it; or leave
`Function` alone and give the type to the HALs only. The second is what this document plans.

Open: the name, the `one-shot` operation of Part D, and whether the extra slot's storage is
acceptable for the large `ExtraSize` callbacks.

### Part C — HAL callbacks with the same defect

A callback qualifies when an interrupt invokes it and the thread assigns it while the interrupt
can fire. Surveyed in `infra/hal/ti/hal_tiva/tiva` and `infra/hal/st/hal_st/stm32fxxx`:

| Driver | Member | Assigned | Defect |
|--------|--------|----------|--------|
| TI `Can` | `onSendComplete`, `onReceive` | every `SendData` / `ReceiveData` | The send-complete interrupt of the previous frame can land in the assignment for the next. This is the likeliest to bite: it is called per frame |
| TI `Adc` | `callback` | every `Measure` | The case already worked around in e-foc by handing the converter one callback once |
| TI `AnalogComparator` | `onOutputChanged` | `Enable`, and `nullptr` in `Disable` | Disabling while the comparator fires |
| TI and ST `Gpio` | `handlers[]` | `EnableInterrupt`, `nullptr` on disable | The interrupt tests `if (handlers[h])` and then calls: a check-then-act on a function that can change in between |
| ST `AdcDmaStm`, `AnalogToDigitalPinStm` | `onDone` (`AutoResetFunction`) | every `Measure` | Same as the TI ADC, and the interrupt resets it |
| TI and ST `WatchDog` | `onEarlyWarning` | `Start` | Assigned once before the interrupt is enabled: safe |
| TI `Pwm`, ST DMA stream handlers | constructor arguments | Construction | Safe: never reassigned |

Order: `Can`, `Adc`, `Gpio`, `AnalogComparator`, then the ST equivalents. Each is its own change
in the HAL repository with a test that fires the handler from inside the assignment, which the
host build can do by calling the handler from a copy hook. The `AutoResetFunction` members need
the one-shot operation: take the callback and clear it in one step. On a core with exclusive
loads that is an atomic exchange of the index; on one without, it is the case Part D forbids and
needs a short critical section instead.

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
