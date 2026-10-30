# GitHub Copilot Instructions — e-foc

All project rules are in **[AGENTS.md](../AGENTS.md)** (shared with Claude; VS Code auto-loads it). Do not restate them here.

Copilot-specific files:
- `.github/instructions/*.instructions.md` — applied by `applyTo` glob; they add file-type specifics only (CMake targets, document contents, embedded C++ scope, test patterns)
- `.github/agents/*.agent.md` — orchestrator, planner, executor, reviewer and analyst agents with their handoff buttons
- `.github/prompts/orchestrate.prompt.md` — starts the orchestrated plan-execute-review workflow
