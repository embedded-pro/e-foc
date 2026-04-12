---
description: "e-foc C++ coding rules for embedded/real-time code: no heap allocation, bounded containers, real-time determinism, FOC theory correctness (Clarke/Park/SVM), motor control best practices, embedded compiler optimizations, Allman brace style, PascalCase naming, SOLID principles, const correctness, documentation alignment."
applyTo: core/{foc,platform_abstraction,state_machine}/**,targets/**
---

# e-foc C++ Rules

> **Scope**: These rules apply to the embedded and real-time code paths (`core/foc/`, `core/platform_abstraction/`, `targets/`). Host-side tooling (`tools/`, `core/services/`) may use standard library heap allocation where appropriate for a host environment.

This project is a Field-Oriented Control (FOC) implementation for BLDC/PMSM motors targeting resource-constrained embedded microcontrollers. Follow these rules strictly within the scoped paths.

## Memory — No Heap Allocation

Never use `new`, `delete`, `malloc`, `free`, `std::make_unique`, or `std::make_shared`.

Avoid `virtual ~ClassName() = 0` (pure virtual destructors) — they pull in `__cxa_pure_virtual` and vtable overhead, increasing flash/RAM usage significantly. The default is **no pure virtual destructor**. Only add one when there is a proven, documented need.

Replace standard containers:
- `std::vector<T>` → `infra::BoundedVector<T>::WithMaxSize<N>`
- `std::string` → `infra::BoundedString::WithStorage<N>`
- `std::deque<T>` → `infra::BoundedDeque<T>::WithMaxSize<N>`
- `std::list<T>` → `infra::BoundedList<T>::WithMaxSize<N>`
- Use `std::array<T, N>` for fixed-size arrays
- No recursion — stack usage must be predictable

## Real-Time — FOC Loop Rules

The `Calculate()` method runs at 20 kHz in interrupt context. Every cycle matters.

- No virtual dispatch in `Calculate()` hot path
- No blocking calls in any ISR-reachable code
- Target: <400 cycles at 120 MHz for the full FOC loop
- Use `TrigonometricImpl` or lookup tables — not raw `sin`/`cos` in hot paths

Every implementation file with hot-path code MUST include:

```cpp
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif
```

Apply `OPTIMIZE_FOR_SPEED` (from `numerical/math/CompilerOptimizations.hpp`) on `Calculate()`, `Compute()`, and other hot-path methods.

## FOC Theory — Correctness

- **Clarke**: `Iα = Ia`, `Iβ = (Ia + 2·Ib) / √3`
- **Park**: `Id = Iα·cos(θ) + Iβ·sin(θ)`, `Iq = -Iα·sin(θ) + Iβ·cos(θ)`
- **Electrical angle**: `θe = θm · pole_pairs`
- **Anti-windup**: All PID integrators must have clamping or back-calculation
- Reuse `TransformsClarkePark` and `SpaceVectorModulation` — do not duplicate

Use unit-typed aliases throughout: `Ampere`, `Radians`, `Volts`, `Rpm`, `PhasePwmDutyCycles`, `PhaseCurrents`.

## Naming

- Classes/Methods: `PascalCase` (e.g., `FocSpeedImpl`, `Calculate()`)
- Member variables: `camelCase` (e.g., `polePairs`, `currentTunings`)
- Namespaces: lowercase (`foc`, `hardware`)

## Style

- Allman braces (opening brace on new line), 4-space indent
- Functions ~30 lines max (hard limit ~50)
- `Calculate()` delegates to focused helper methods
- Self-documenting code — avoid unnecessary comments
- No implementation in headers — only templated classes may have their implementation in the header file. All other method bodies go in `.cpp` files.
- `const` on all non-mutating methods, `constexpr` where possible
- Fixed-size types: `uint8_t`, `int32_t`, etc.
- Prefer `{}` initialization over `()` for variables and member data (e.g., `float x{0.0f}`, `std::size_t n{0}`)

## Design

- SOLID principles — constructor injection, one class = one control concern
- Platform dependencies injected via constructor and `PlatformFactory`
- New FOC modes implement `FocTorque`, `FocSpeed`, or `FocPosition` interfaces
- Reuse `infra/numerical-toolbox/` for PID, filters, and math — do not duplicate
- RAII for resource management

## Documentation — MANDATORY

**Documentation-first rule**: If a change alters any component's observable behavior, the corresponding architecture or design document in `documentation/` must be updated **before or alongside** the code. Code must follow documentation, not the opposite. If no architecture/design document exists for the affected component, create one using `documentation/templates/architecture.md` or `documentation/templates/design.md` before writing production code.

For every FOC algorithm or motor model change, update the corresponding `documentation/theory/{topic}.md`. For timing-critical changes, update `documentation/performance-optimization/README.md`.

Within architecture and design documentation files, all visuals must be Mermaid code blocks (`

Full details: [copilot-instructions.md](../../.github/copilot-instructions.md), [documentation/theory/foc.md](../../documentation/theory/foc.md)
