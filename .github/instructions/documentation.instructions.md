---
description: "e-foc documentation rules: documentation-first for behavioral changes, Mermaid/TikZ/ASCII visuals plus generated animations in theory, theory/architecture/design document types, template usage."
applyTo: "documentation/**"
---

# e-foc Documentation Rules

Follow AGENTS.md — see §Documentation (documentation-first, which folder to update, templates, allowed visuals, TikZ previews, generated animations). This file adds only the document specifics.

## Image syntax

The only image references allowed, each from a document one folder below `documentation/`:
- TikZ preview, next to its ` ```{=latex} ` block: `![alt](../tikz/images/<name>.svg)`
- Animation, in `documentation/theory/` only: `![alt](../animations/<name>.svg)`

## Content requirements

Each document should include:
- Mathematical background (equations for transforms, PID tuning, etc.)
- Control-loop diagram (Mermaid or ASCII)
- Hardware dependencies
- Tuning guidance where applicable
