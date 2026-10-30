# e-foc — Claude Instructions

@AGENTS.md

Claude-specific notes (every project rule is in AGENTS.md, imported above):
- **Sub-agents** — definitions in `.claude/agents/`. Use `analyst` for investigations; `planner` for design; `executor` for implementation; `reviewer` for review.
- **Skills** — load the `ux-ui-expert` skill for any user-facing change under `tools/`.
- **Be terse** — minimal prose; report file paths + pass/fail.
