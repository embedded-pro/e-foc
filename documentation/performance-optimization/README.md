---
title: "Embedded Performance Optimization"
type: theory
status: approved
version: 1.0.0
component: "foc"
date: 2026-04-07
---

| Field     | Value                              |
|-----------|------------------------------------|
| Title     | Embedded Performance Optimization  |
| Type      | theory                             |
| Status    | approved                           |
| Version   | 1.0.0                              |
| Component | foc                                |
| Date      | 2026-04-07                         |

## Overview

Real-time motor control firmware must meet strict cycle-count deadlines at every control-loop iteration.
On an ARM Cortex-M4F running at 120 MHz with a 20 kHz FOC loop, the complete ISR computation budget is
6 000 cycles — fewer than 400 of which should be consumed by the FOC core (Clarke, Park, two PI
controllers, inverse Park, SVM). Violating this budget causes PWM update skips, current ripple, and
control instability.

This document describes the compiler and code-level techniques used to keep the FOC path within budget,
how to measure and verify cycle usage, and the common patterns that silently erode performance.

---

## Prerequisites

| Symbol   | Meaning                              | Unit   |
|----------|--------------------------------------|--------|
| $f_{cpu}$| CPU clock frequency                  | Hz     |
| $f_{sw}$ | Switching (PWM) frequency            | Hz     |
| $N_{cyc}$| Cycles available per control period  | cycles |
| CPI      | Cycles Per Instruction (pipeline)    | —      |
| FMA      | Fused Multiply-Accumulate            | —      |
| LTO      | Link-Time Optimisation               | —      |

$$
N_{cyc} = \frac{f_{cpu}}{f_{sw}} = \frac{120\,\text{MHz}}{20\,\text{kHz}} = 6000\ \text{cycles}
$$

---

## Mathematical Foundation

### ARM Cortex-M4 Instruction Pipeline

The Cortex-M4 uses a 3-stage pipeline (fetch, decode, execute) with a dual-issue capability for
certain instruction pairs.  The FPU (FPv4-SP) adds a separate pipelined floating-point execution
unit.  Key throughput properties:

| Instruction          | Description               | Latency (cycles) | Throughput (cycles/inst) |
|----------------------|---------------------------|-----------------|--------------------------|
| `vfma.f32`           | Fused multiply-add        | 1               | 1                        |
| `vmul.f32`           | Float multiply            | 1               | 1                        |
| `vadd.f32`           | Float add                 | 1               | 1                        |
| `vdiv.f32`           | Float divide              | 14              | 14                       |
| `vsqrt.f32`          | Float sqrt                | 14              | 14                       |
| `vcmpe.f32`          | Float compare             | 1               | 1                        |
| `blx rN`             | Indirect call (virtual)   | 3+              | 3+                       |
| `bl <addr>`          | Direct call               | 1+N             | 1+N                      |
| `push/pop (4 regs)`  | Stack save/restore        | 2               | 2                        |

**Implication for FOC**: All hot-path arithmetic should use FMA-capable patterns. Division and sqrt
must be avoided in the ISR where possible; use reciprocals or LUT-based approximations instead.

### Compiler Optimisation Theory

The GCC/Clang compiler applies transformations that can drastically change cycle count:

1. **Inlining**: For short functions called in tight loops, inlining eliminates the call overhead
   (branch, stack frame setup/teardown), and enables cross-function optimisations such as constant
   propagation and dead-code elimination.

2. **Loop unrolling**: The compiler repeats loop body $k$ times to reduce branch overhead and expose
   more instruction-level parallelism.  Controlled by `-funroll-loops` or `#pragma GCC optimize("O3")`.

3. **Constant propagation / folding**: Compile-time evaluation of constant expressions eliminates
   runtime computation.  Use `constexpr` aggressively for lookup tables and configuration constants.

4. **Auto-vectorisation**: Cortex-M4 SIMD (DSP extensions) can process two 16-bit or four 8-bit
   operands per cycle.  Q15/Q31 fixed-point code benefits; floating-point does not (no NEON on M4).

5. **Fast-math transformations** (`-ffast-math`, `#pragma GCC optimize("fast-math")`):
   - Allows re-association of floating-point expressions.
   - Enables FMA generation (avoids separate multiply + add).
   - Assumes no NaN/Inf inputs.
   - **May change numerical results slightly** — verify with tests.

### Virtual Dispatch Cost Model

A virtual function call requires:
1. Load vtable pointer from `this` (1 load, possible cache miss).
2. Load function pointer from vtable (1 load, possible i-cache miss).
3. Indirect branch `blx rN` (3+ cycles, branch predictor cannot speculate).
4. Callee setup/teardown (push/pop, stack frame).

Total overhead: 6–20+ cycles per call depending on cache state.  At 40 ns per virtual call (120 MHz),
10 virtual calls per FOC iteration consume 400 ns — approximately 6.7 % of the 20 kHz budget.

For the FOC path, prefer static dispatch (non-virtual, `inline`, `constexpr`) for all computation
that occurs inside the ISR.

---

## Optimisation Techniques

### 1. Avoid Virtual Functions in Hot Paths

**Avoid** (virtual dispatch overhead):
```cpp
class ITrigonometry {
public:
    virtual float Sine(float angle) const = 0;
};
// In hot path: vtable lookup + indirect call
float sin_val = trig->Sine(angle);
```

**Prefer** (static dispatch, inlinable):
```cpp
struct FastTrigonometry {
    static inline float Sine(float angle) noexcept {
        return LookupTable[index];  // Direct access, fully inlined
    }
};
float sin_val = FastTrigonometry::Sine(angle);
```

### 2. Avoid `std::optional` in Performance-Critical Code

```cpp
// Avoid: generates has_value() checks and extra memory access
std::optional<float> setPoint;

// Prefer: simple flag
float setPointValue = 0.0f;
bool hasSetPoint = false;
if (!hasSetPoint) [[unlikely]] return;
```

### 3. Use `constexpr` and `inline` Aggressively

```cpp
// Computed entirely at compile time — zero runtime cost
inline constexpr std::array<float, 512> sineLUT = []() {
    std::array<float, 512> table{};
    for (size_t i = 0; i < 512; ++i)
        table[i] = std::sin(2.0f * M_PI * i / 512.0f);
    return table;
}();
```

### 4. Use Compiler Attributes

```cpp
#define ALWAYS_INLINE  __attribute__((always_inline)) inline
#define HOT_FUNCTION   __attribute__((hot))

// Combined: force inlining + speed-optimised
__attribute__((always_inline, hot)) inline
void FocStep(FocState& s) { /* ... */ }
```

### 5. Prefer Fixed-Size Integer Types

```cpp
// Good: Explicit widths, portable
uint32_t counter;
int16_t  current_mA;
float    voltage_V;

// Avoid: Implementation-defined widths
int counter;
short current;
```

### 6. Minimise Stack Usage

```cpp
// Avoid: 4 KB on stack inside a function
void Calculate() {
    float buffer[1024];
}

// Prefer: static member in .bss
class Calculator {
    static float buffer[1024];  // Zero-init, no stack cost
};
```

### 7. Enable FMA Patterns

With `-ffast-math`, the compiler emits `vfma.f32` for `a * b + c` patterns:

```cpp
// This single expression:
result = a * b + c;
// Becomes: vfma.f32 s0, s1, s2  (1 cycle, full precision)
```

### 8. Per-File Optimisation Pragmas

For performance-critical translation units (FOC implementations, SVM, transforms):

```cpp
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3", "fast-math")
#endif
```

Place at the top of the `.cpp` file, before any includes that define affected functions.

### 9. Per-Function Attributes

```cpp
__attribute__((optimize("-O3")))
void CriticalFunction() {
    // Always compiled at -O3 regardless of TU-level flags
}
```

Note: function attributes do not propagate to callees.  Use file-level pragmas for transitive effect.

---

## Debug Builds

By default, Debug builds (`-O0`) disable all optimisations, making code 3–10× slower than Release.
This breaks real-time deadlines for FOC ISRs during debugging sessions.

**Solution 1 — Use `-Og` for debug builds** (recommended):

```cmake
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g" CACHE STRING "Debug flags" FORCE)
```

`-Og` provides: basic inlining, dead-code elimination, register allocation — while keeping full
debuggability (variable inspection, correct stack trace).

**Solution 2 — File-level pragma** (when specific files must remain fast even at -O0):

```cpp
#pragma GCC optimize("O3", "fast-math")
```

**Solution 3 — Function attribute** (for a single bottleneck function):

```cpp
__attribute__((optimize("-O3")))
void CriticalFunction() { /* ... */ }
```

---

## Analysing Generated Code

### Disassembly with objdump

```bash
# With C++ demangling + source interleave (requires -g)
arm-none-eabi-objdump -d -S -C firmware.elf > disassembly_with_source.txt

# Just one section
arm-none-eabi-objdump -d -j .text file.elf

# Specific function (grep)
arm-none-eabi-objdump -d -C firmware.elf | grep -A 100 "FunctionName"
```

### Size Analysis

```bash
# Section sizes
arm-none-eabi-size firmware.elf

# Top 20 largest symbols
arm-none-eabi-nm --size-sort -C firmware.elf | tail -20
```

### Signs of Poor Optimisation

```asm
; Virtual dispatch footprint
ldr    r3, [r0, #0]   ; Load vtable pointer
ldr    r3, [r3, #4]   ; Load function pointer
blx    r3             ; Indirect call — 3+ cycles, unpredictable branch

; Division (avoid in ISR)
vdiv.f32  s0, s1, s2  ; 14 cycles

; Repeated memory loads (register allocation failure)
ldr    r3, [r7, #4]
; ... some code ...
ldr    r3, [r7, #4]   ; Same address loaded again
```

### Signs of Good Optimisation

```asm
; Minimal stack frame
push   {r4, r5, lr}
sub    sp, #16

; FMA instruction
vfma.f32  s0, s1, s2   ; 1 cycle

; Conditional execution — no branch
vcmpe.f32  s0, s1
it         gt
vmovgt.f32 s0, s1
```

---

## Measuring Performance

### Cycle Counter (DWT)

```cpp
// Enable at startup (once)
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL   |= DWT_CTRL_CYCCNTENA_Msk;

// Measure
uint32_t start  = DWT->CYCCNT;
FocCoreStep();
uint32_t cycles = DWT->CYCCNT - start;
```

### GPIO Toggle (requires oscilloscope)

```cpp
GPIO_SetPin(DEBUG_PIN);
FocCoreStep();
GPIO_ClearPin(DEBUG_PIN);
// Measure pulse width on scope → cycles = width × f_cpu
```

---

## Numerical Properties

### Control Loop Cycle Budgets — ARM Cortex-M4 at 120 MHz

| Control Rate | Cycles/Period | FOC Budget (< 7 %) | Notes                          |
|-------------|--------------|---------------------|--------------------------------|
| 10 kHz      | 12 000       | 840                 | Low-frequency, conservative    |
| 20 kHz      | 6 000        | **420**             | Target rate for this project   |
| 40 kHz      | 3 000        | 210                 | High performance, tight budget |
| 100 kHz     | 1 200        | 84                  | Requires fixed-point or DSP    |

**FOC core typical requirements**:
- Optimised (`-O3`, `fast-math`, LUT sin/cos): **200–400 cycles**
- Non-optimised (`-O0`, `sinf/cosf` calls): **800–1500 cycles**

### Key Performance Sensitivities

| Code Pattern             | Cycle Impact       | Recommendation                         |
|--------------------------|--------------------|-----------------------------------------|
| `sinf()` / `cosf()`      | 50–200 cycles each | Replace with 512-entry LUT              |
| Virtual call in ISR      | 6–20 cycles each   | Use static dispatch / inline            |
| `vdiv.f32`               | 14 cycles          | Use precomputed reciprocal              |
| `vsqrt.f32`              | 14 cycles          | Use fast inverse sqrt approximation     |
| Stack frame > 64 bytes   | 2–4 cycles extra   | Reduce local variables                  |
| Cache miss (data)        | 3–10 cycles        | Keep hot data in registers or TCM       |

### Common Pitfalls

| Problem                    | Symptom                         | Fix                                     |
|----------------------------|---------------------------------|-----------------------------------------|
| Heap allocation in ISR     | Non-deterministic latency spike | Use static/stack allocation only        |
| `printf` in ISR            | 10 000+ cycles, UART block      | Use DWT counter or GPIO toggle instead  |
| Exception handling overhead| Increased code size, slow paths | Compile with `-fno-exceptions -fno-rtti`|
| Float in integer-only code | Unnecessary FPU state save      | Use integer arithmetic or explicit cast |
| Packed struct with uint32  | Unaligned access fault or stall | Align struct members on natural boundary|

---

## Quick Reference

### GCC Optimisation Pragmas

```cpp
#pragma GCC optimize("O3")        // Maximum speed
#pragma GCC optimize("Os")        // Minimum size
#pragma GCC optimize("fast-math") // Aggressive FP
#pragma GCC push_options          // Save current options
#pragma GCC pop_options           // Restore options
```

### Function Attributes

```cpp
__attribute__((always_inline))   // Force inline
__attribute__((noinline))        // Prevent inline
__attribute__((hot))             // Optimise for speed
__attribute__((cold))            // Optimise for size
__attribute__((pure))            // No side effects
__attribute__((const))           // Pure + no memory reads
__attribute__((flatten))         // Inline all callees
```

### Branch Hints

```cpp
if (condition) [[likely]]   { }   // C++20
if (condition) [[unlikely]] { }   // C++20
```

---

## References

1. ARM — *Cortex-M4 Technical Reference Manual*, ARM DDI 0439B.
2. ARM — *Cortex-M4 Devices Generic User Guide*, ARM DUI 0553A.
3. Fog, A. — *Optimizing software in C++*, Technical University of Denmark, 2023.
4. GCC Project — *Optimize Options*, GCC Manual, https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
5. Texas Instruments — *AM335x ARM Cortex-A8 Microprocessors Technical Reference Manual*, 2019.
