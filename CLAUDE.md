# e-foc — Claude Instructions

Canonical rules: **[AGENTS.md](AGENTS.md)** (shared with Copilot and sub-agents).
Sub-agent definitions: `.claude/agents/`. Build presets: `CMakePresets.json`.

Essentials (full detail in AGENTS.md):
- **No heap** — bounded containers / `std::array` / `std::optional`; no recursion; no `virtual ~D() = 0`; tests too. Scope: `core/foc/`, `core/platform_abstraction/`, `core/state_machine/`, `targets/`, ISR paths.
- **Real-time** — `Calculate()` at 20 kHz; ≤4500 cycles at 120 MHz. No virtual dispatch, no heap, no blocking, no raw `sin`/`cos` in hot path. `#pragma GCC optimize("O3","fast-math")` + `OPTIMIZE_FOR_SPEED`.
- **FOC theory** — Clarke/Park/SVM from `core/foc/transforms/`; reuse, don't reimplement. Anti-windup on all PIDs. Unit types: `Ampere`, `Radians`, `Volts`, `RevPerMinute`.
- **Style** — Allman braces, 4-space, `{}` init, PascalCase types/methods, camelCase members. No comments except non-obvious *why*.
- **Tests** — `TEST_F`, `StrictMock` only, `EXPECT_NEAR`, no heap. `NiceMock`/`NaggyMock` forbidden.
- **No exceptions** — `std::optional`/status enums; interfaces `virtual ~I() = default`.
- **Docs-first** — update `documentation/` before/alongside behavioral changes. Mermaid/ASCII only.
- **CI** — see AGENTS.md §Build for the two-tier workflow and embedded cmake rules.
- **Agents** — see AGENTS.md §Agent routing. Use `analyst` for investigations; `planner` for design; `executor` for implementation; `reviewer` for review.
- **Be terse** — minimal prose; report file paths + pass/fail.
