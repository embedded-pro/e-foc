---
description: "Start an orchestrated plan-execute-review workflow for an e-foc development task. Routes through planning, implementation, and code review stages with handoff buttons between each step."
agent: "orchestrator"
argument-hint: "Describe the FOC feature, motor control algorithm, bug fix, or change you want to implement"
model: claude-sonnet
---

# Orchestrate Workflow

Analyze the following task for the **e-foc** project (rules: AGENTS.md). Gather relevant context from the
codebase — identify affected layers (AGENTS.md §Architecture), the control mode (torque/speed/position),
real-time timing implications (§Real-time), and documentation requirements (§Documentation). Then provide a
brief scope summary and use the handoff buttons to route to the appropriate specialist:

- **Plan Implementation**: For complex tasks needing detailed upfront design (new FOC modes, new transforms, architectural changes, parameter identification algorithms)
- **Execute Directly**: For straightforward changes with a clear path (bug fixes, small tuning changes, adding tests)
- **Review Code**: For reviewing existing or recently changed code against project standards
- **Investigate**: For analysis, audits, root-cause investigation, or design exploration (no code changes)

Task to orchestrate:
