---
description: "e-foc documentation rules: documentation-first for behavioral changes, Mermaid/TikZ/ASCII visuals plus generated animations in theory, theory/architecture/design/verification_and_validation folders, document structure, PDF layout, template usage."
applyTo: "documentation/**"
---

# e-foc Documentation Rules

Follow AGENTS.md — see §Documentation (documentation-first, which folder to update, templates, allowed visuals, TikZ previews, generated animations, and the document rules: front matter, Responsibilities without a NOT list, no Open Questions, no microcontroller in design documents, the PDF front matter and the release zip layout). This file adds only the document specifics.

## Image syntax

The only image references allowed, each from a document one folder below `documentation/`:
- TikZ preview, next to its ` ```{=latex} ` block: `![alt](../tikz/images/<name>.svg)`
- Animation, in `documentation/theory/` only: `![alt](../animations/<name>.svg)`

## Content requirements

Each document should include:
- Mathematical background (equations for transforms, PID tuning, etc.)
- Control-loop diagram (Mermaid or ASCII)
- Hardware dependencies, as capabilities the platform provides (theory and performance documents may name parts; design and architecture documents do not)
- Tuning guidance where applicable
