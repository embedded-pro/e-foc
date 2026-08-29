---
description: "Use when implementing code changes in e-foc. Writes production code and tests following all project constraints: no heap allocation, bounded containers, real-time determinism, FOC theory correctness, motor control best practices, SOLID principles, and documentation alignment."
tools: [read, edit, search, execute, todo]
model: claude-sonnet
handoffs:
  - label: "Review Changes"
    agent: reviewer
    prompt: "Review the implementation changes made above against e-foc project standards."
---

# Executor Agent

Implement code. Follow all constraints in AGENTS.md without exception.

## Workflow (TDD Red-Green-Refactor)

1. **Clarify** — expected inputs/outputs, edge cases, control mode, hardware target, acceptance criteria
2. **Read the plan** — understand FOC theory context before touching any file
3. **Find patterns** — search `core/foc/` for existing patterns; follow them exactly
4. **Red** — write failing tests first in `test/Test{ComponentName}.cpp` for every behavior
5. **Green** — implement minimum production code to pass tests, one file at a time
6. **Hot-path** — add `#pragma GCC optimize` + `OPTIMIZE_FOR_SPEED` to all hot-path code
7. **Refactor** — clean up while keeping all tests green
8. **CMake** — update `CMakeLists.txt` if new files added
9. **Docs** — update `documentation/` for every algorithm or procedure added/changed
10. **Verify** — `cmake --build --preset host-Debug` then `ctest --preset host`
11. **Hand off** — use "Review Changes" to trigger reviewer

## Do NOT

- Add features beyond what was requested; refactor code unrelated to the task
- Reimplement Clarke, Park, or SVM — use `TransformsClarkePark`/`SpaceVectorModulation`
- Use plain `float` where unit-typed aliases (`Ampere`, `Radians`, `Volts`) exist
- Add comments that restate what the code does

All rules (memory, real-time, FOC theory, style, testing) are in AGENTS.md and the auto-injected instructions files — do not repeat them; just follow them.
