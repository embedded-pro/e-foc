---
description: "e-foc C++ coding rules for embedded/real-time code: no heap allocation, bounded containers, real-time determinism, FOC theory correctness (Clarke/Park/SVM), motor control best practices, embedded compiler optimizations, Allman brace style, PascalCase naming, SOLID principles, const correctness, documentation alignment."
applyTo: "core/{foc,platform_abstraction,state_machine}/**,targets/sync_foc_sensored/**,targets/hardware_test/**,targets/platform_implementations/{ti,st,error_handling_cortex_m}/**"
---

# e-foc C++ Rules

These paths are embedded and real-time code: the no-heap scope of AGENTS.md §Memory applies in full here.

Follow AGENTS.md — see §Memory, §Real-time, §FOC theory, §Style, §Interfaces & errors, §Async callback lifetime safety and §Documentation. Theory background: [documentation/theory/foc.md](../../documentation/theory/foc.md).
