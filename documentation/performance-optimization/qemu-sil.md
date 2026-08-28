---
title: "QEMU SIL Cycle Measurement"
type: theory
status: approved
version: 1.0.0
component: "foc"
date: 2026-08-28
---

| Field     | Value                      |
|-----------|----------------------------|
| Title     | QEMU SIL Cycle Measurement |
| Type      | theory                     |
| Status    | approved                   |
| Version   | 1.0.0                      |
| Component | foc                        |
| Date      | 2026-08-28                 |

## Overview

Three complementary measurement tiers are used to verify FOC cycle budgets:

```
┌─────────────────────────────────────────────────────────────────┐
│  Tier 1 — Static estimate (merge gate)                          │
│  Tool   : cortex-cycle-budget GitHub Action                     │
│  Budget : ≤ 4500 cycles inner loop / ≤ 20000 outer loop         │
│  When   : Every CI run on embedded_build job                    │
│  Runs on: ELF file only — no execution                          │
├─────────────────────────────────────────────────────────────────┤
│  Tier 2 — QEMU SIL (regression signal)          ← this doc      │
│  Tool   : qemu-system-arm + DWT CYCCNT                          │
│  Budget : relative regression gate (not absolute silicon budget) │
│  When   : Every CI run on qemu_sil_tests job (matrix M4 / M7)   │
│  Runs on: ARM ELF under emulated Cortex-M4 / M7                 │
├─────────────────────────────────────────────────────────────────┤
│  Tier 3 — On-silicon DWT (ground truth)                         │
│  Tool   : PlatformFactory::ElapsedCycles() via TIVA HIL         │
│  Budget : authoritative absolute measurement                    │
│  When   : Hardware-in-the-loop test suite                       │
│  Runs on: Physical EK-TM4C1294XL board                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## QEMU machine-model mapping

| CMake preset     | QEMU machine | Emulated core | Toolchain                          |
|------------------|--------------|---------------|------------------------------------|
| `qemu-cortex-m4` | `mps2-an386` | Cortex-M4F    | `toolchain-arm-gcc-m4-fpv4-sp-d16` |
| `qemu-cortex-m7` | `mps2-an500` | Cortex-M7F    | `toolchain-arm-gcc-m7-fpv5-d16`    |

---

## Mathematical Foundation

### DWT CYCCNT model under TCG

On real Cortex-M silicon the Data Watchpoint and Trace (DWT) `CYCCNT` register increments once per processor clock cycle. QEMU executes ARM ELFs through its Tiny Code Generator (TCG) backend, which translates ARM instruction blocks to the host ISA at runtime. Under TCG the CYCCNT model is:

$$
\text{CYCCNT}_\text{TCG}(I) = |I|
$$

where $|I|$ is the count of retired ARM instructions in the executed instruction stream $I$. Each ARM instruction advances CYCCNT by exactly **1**, independent of instruction latency, FPU pipeline depth, or memory access cost.

### Regression gate formula

Let $C_\text{base}$ be the CYCCNT from the target branch and $C_\text{PR}$ be the CYCCNT from the pull-request branch for the same benchmark workload. The CI regression gate flags a failure when:

$$
\frac{C_\text{PR} - C_\text{base}}{C_\text{base}} > \delta
$$

where $\delta$ is the regression threshold (default 5 %). This is a *relative* gate: it detects instruction-count growth, not absolute silicon timing.

### Cross-core instruction-count delta

For two targets $T_\text{M4}$ (Cortex-M4F, `mps2-an386`) and $T_\text{M7}$ (Cortex-M7F, `mps2-an500`) executing the same binary compiled for each core, the CYCCNT difference reflects ISA path differences:

$$
\Delta C = C_\text{M7} - C_\text{M4}
$$

A negative $\Delta C$ indicates the M7 toolchain generated fewer instructions (e.g. by using double-precision FPU paths or SIMD extensions unavailable on M4).

---

## Numerical Properties

| Property              | Value / Condition                                                         |
|-----------------------|---------------------------------------------------------------------------|
| Resolution            | 1 count per retired ARM instruction                                       |
| Counter width         | 32-bit, wraps at $2^{32}$                                                 |
| Determinism           | 100 % — same binary + inputs always produces the same CYCCNT              |
| Silicon fidelity      | None — does not model pipeline stalls, FPU latency, or cache miss penalty |
| Regression sensitivity | ~1 % instruction-count change is detectable                              |
| Absolute budget use   | Not valid — use Tier 1 (static) or Tier 3 (on-silicon) instead           |

---

## Critical caveat: CYCCNT is not cycle-accurate under QEMU

QEMU's TCG executes ARM instructions by translating them to host ISA blocks.
It does **not** model:

- Pipeline stalls or forwarding
- FPU latency differences between M4 (FPv4-SP) and M7 (FPv5)
- Cache miss penalties
- Branch prediction

As a result, `DWT->CYCCNT` under QEMU increments roughly once per retired instruction, not once per
real MCU clock cycle. A measured CYCCNT of N on `mps2-an386` does **not** mean N cycles on a real
Cortex-M4.

**Correct uses of QEMU cycle counts:**

1. **ARM ISA correctness** — confirms the code runs on a real ARM core including VFP hard-float
   semantics that x86 host tests cannot exercise.
2. **Relative regression detection** — a PR that increases CYCCNT by 20 % has almost certainly added
   instructions. Pair with CI diff.
3. **Cross-core instruction-count delta** — M4 vs M7 CYCCNT differences reflect instruction-count
   differences (e.g. double-precision FPU path taken on M7).

**Incorrect use:**

- Comparing QEMU CYCCNT against the 4500-cycle silicon budget. Use Tier 1 or Tier 3 for that.

---

## What runs under QEMU

### Unit test suites (Phase 1)

Seven hot-path gtest suites are linked with the QEMU runtime and executed under emulation:

| Target                         | Tests                                       |
|--------------------------------|---------------------------------------------|
| `e_foc.foc.transforms_test`    | Clarke, Park, SVM                           |
| `e_foc.foc.current_loop_test`  | PID, deadbeat, sliding-mode current control |
| `e_foc.foc.cascade_test`       | Torque, speed, position cascade             |
| `e_foc.foc.math_test`          | Angle wrap, fast trigonometry               |
| `e_foc.foc.speed_loop_test`    | PID, ADRC, LQI, two-DOF speed control       |
| `e_foc.foc.position_loop_test` | PID, LQI, LQR, two-DOF position control     |
| `e_foc.foc.selection_test`     | Controller selector                         |

### Cycle benchmark harness (Phase 2)

`e_foc.sil_qemu.cycle_bench` (`integration_tests/software_in_the_loop_qemu/`) measures `Calculate()`
of the three cascade levels using `hal::cortex::DataWatchpointAndTrace` (DWT CYCCNT).

Output format written to semihosting stdout (captured in `LastTest.log`):
```
[[CYCLES]] torque_calculate=NNN
[[CYCLES]] speed_calculate=NNN
[[CYCLES]] position_calculate=NNN
```

The CI `qemu_sil_tests` job parses these lines and posts them to the GitHub step summary as a table.

---

## Running locally

```bash
# Configure
cmake --preset qemu-cortex-m4

# Build explicit targets (full project all won't work cross-compiled)
cmake --build --preset qemu-cortex-m4-RelWithDebInfo

# Run tests via ctest (invokes each ELF under QEMU automatically)
ctest --preset qemu-cortex-m4

# Same for M7
cmake --preset qemu-cortex-m7
cmake --build --preset qemu-cortex-m7-RelWithDebInfo
ctest --preset qemu-cortex-m7
```

Requires `qemu-system-arm` with `mps2-an386` and `mps2-an500` machines.
The devcontainer image `gabrielfrasantos/embedded-devcontainer-cpp:v7.3.0` ships both.

---

## References

- [performance-optimization/README.md](README.md) — compiler optimisation techniques and Tier 1/3 details
- `infra/embedded-infra-lib/hal/cortex_m/DataWatchpointAndTrace.hpp` — DWT CYCCNT API
- `infra/embedded-infra-lib/hal/qemu/` — QEMU HAL (semihosting, startup, linker script)
- `infra/numerical-toolbox/cmake/QemuHelpers.cmake` — reference implementation this design follows
